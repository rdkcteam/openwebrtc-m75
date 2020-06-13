// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

#include <map>

#include "mojo/public/cpp/bindings/associated_binding.h"

namespace blink {

class AssociatedInterfaceProvider::LocalProvider
    : public mojom::AssociatedInterfaceProvider {
 public:
  using Binder =
      base::RepeatingCallback<void(mojo::ScopedInterfaceEndpointHandle)>;

  explicit LocalProvider(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : associated_interface_provider_binding_(this) {
    associated_interface_provider_binding_.Bind(
        mojo::MakeRequestAssociatedWithDedicatedPipe(&ptr_),
        std::move(task_runner));
  }

  ~LocalProvider() override {}

  void SetBinderForName(const std::string& name, const Binder& binder) {
    binders_[name] = binder;
  }

  bool HasInterface(const std::string& name) const {
    return binders_.find(name) != binders_.end();
  }

  void GetInterface(const std::string& name,
                    mojo::ScopedInterfaceEndpointHandle handle) {
    return ptr_->GetAssociatedInterface(
        name, mojom::AssociatedInterfaceAssociatedRequest(std::move(handle)));
  }

 private:
  // mojom::AssociatedInterfaceProvider:
  void GetAssociatedInterface(
      const std::string& name,
      mojom::AssociatedInterfaceAssociatedRequest request) override {
    auto it = binders_.find(name);
    if (it != binders_.end())
      it->second.Run(request.PassHandle());
  }

  std::map<std::string, Binder> binders_;
  mojo::AssociatedBinding<mojom::AssociatedInterfaceProvider>
      associated_interface_provider_binding_;
  mojom::AssociatedInterfaceProviderAssociatedPtr ptr_;

  DISALLOW_COPY_AND_ASSIGN(LocalProvider);
};

AssociatedInterfaceProvider::AssociatedInterfaceProvider(
    mojom::AssociatedInterfaceProviderAssociatedPtr proxy,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : proxy_(std::move(proxy)), task_runner_(std::move(task_runner)) {
  DCHECK(proxy_.is_bound());
}

AssociatedInterfaceProvider::AssociatedInterfaceProvider(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : local_provider_(std::make_unique<LocalProvider>(task_runner)),
      task_runner_(std::move(task_runner)) {}

AssociatedInterfaceProvider::~AssociatedInterfaceProvider() = default;

void AssociatedInterfaceProvider::GetInterface(
    const std::string& name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  if (local_provider_ && (local_provider_->HasInterface(name) || !proxy_)) {
    local_provider_->GetInterface(name, std::move(handle));
    return;
  }
  DCHECK(proxy_);
  proxy_->GetAssociatedInterface(
      name, mojom::AssociatedInterfaceAssociatedRequest(std::move(handle)));
}

void AssociatedInterfaceProvider::OverrideBinderForTesting(
    const std::string& name,
    const base::RepeatingCallback<void(mojo::ScopedInterfaceEndpointHandle)>&
        binder) {
  if (!local_provider_)
    local_provider_ = std::make_unique<LocalProvider>(task_runner_);
  local_provider_->SetBinderForName(name, binder);
}

}  // namespace blink