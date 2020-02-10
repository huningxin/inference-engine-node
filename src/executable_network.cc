#include "executable_network.h"
#include "network.h"

#include <napi.h>
#include <uv.h>

using namespace Napi;
namespace ie = InferenceEngine;

namespace ienodejs {

class ExecnetworkAsyncWorker : public Napi::AsyncWorker {
 public:
  ExecnetworkAsyncWorker(Napi::Env& env,
                         const Napi::Value& network,
                         const Napi::Value& device_name,
                         ie::Core& core,
                         Napi::Promise::Deferred& deferred)
      : env_(env),
        Napi::AsyncWorker(env),
        device_name_(device_name.As<Napi::String>()),
        core_(core),
        deferred_(deferred) {
    network_ = Napi::ObjectWrap<Network>::Unwrap(network.ToObject())->actual_;
  }

  ~ExecnetworkAsyncWorker() {}

  void Execute() {
    try {
      executable_network_ = core_.LoadNetwork(network_, device_name_);
    } catch (const std::exception& error) {
      Napi::AsyncWorker::SetError(error.what());
      return;
    } catch (...) {
      Napi::AsyncWorker::SetError("Unknown/internal exception happened.");
      return;
    }
  }

  void OnOK() {
    Napi::EscapableHandleScope scope(env_);
    Napi::Object obj = ExecutableNetwork::constructor.New({});
    ExecutableNetwork* exec_network =
        Napi::ObjectWrap<ExecutableNetwork>::Unwrap(obj);
    exec_network->actual_ = executable_network_;
    deferred_.Resolve(scope.Escape(napi_value(obj)).ToObject());
  }

  void OnError(Napi::Error const& error) { deferred_.Reject(error.Value()); }

 private:
  ie::CNNNetwork network_;
  ie::Core core_;
  ie::ExecutableNetwork executable_network_;
  std::string device_name_;
  Napi::Env env_;
  Napi::Promise::Deferred deferred_;
};

Napi::FunctionReference ExecutableNetwork::constructor;

void ExecutableNetwork::Init(const Napi::Env& env) {
  Napi::HandleScope scope(env);

  Napi::Function func = DefineClass(env, "ExecutableNetwork", {});

  constructor = Napi::Persistent(func);
  constructor.SuppressDestruct();
}

ExecutableNetwork::ExecutableNetwork(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<ExecutableNetwork>(info) {}

void ExecutableNetwork::NewInstanceAsync(Napi::Env& env,
                                         const Napi::Value& network,
                                         const Napi::Value& dev_name,
                                         ie::Core core,
                                         Napi::Promise::Deferred& deferred) {
  ExecnetworkAsyncWorker* execworker =
      new ExecnetworkAsyncWorker(env, network, dev_name, core, deferred);
  execworker->Queue();
}

}  // namespace ienodejs