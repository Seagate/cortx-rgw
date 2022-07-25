// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=2 sw=2 expandtab ft=cpp

/*
 * GC thread/process synchronization for the CORTX Motr backend
 *
 * Copyright (C) 2022 Seagate Technology LLC and/or its Affiliates
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation. See file COPYING.
 *
 */

#ifndef __MOTR_LOCK__
#define __MOTR_LOCK__

/*
Usage:
  To use locking implemented in this file, the caller needs to instantiate
class "MotrKVLockProvider" to create object. On such object, caller needs
to call "initialize()" function, which in turn creates a global lock index,
using the name specified by the caller. The caller can do this during the
process/thread start-up/initialization; if "initialize()" returns < 0, the
caller is expected to stop further initialization, if locking is esential
in the system. In multi-process environment, each process can call
"MotrKVLockProvider::initialize" during it's start-up, providing the same
name for the global lock index.

After "MotrKVLockProvider" instance is created and initialized successfully,
when the caller wants to acquire a resource, caller can first instantiate
"MotrLock" class, passing it the instance of class "MotrKVLockProvider" in
MotrLock::initialize() method. The caller can call MotrLock::lock() to lock
the resource. Once the caller is done with the resource, caller can 
call "MotrLock::unlock()". Any -ve value returned by MotrLock::lock()
indicates resource can't be locked; a value 0 indicates success.

Approach 1.
Steps for caller(developer):
1. During startup, create instance of class MotrKVLockProvider. Call
initialize() on the instance, passing it a globally unique name for the
global lock index creation. If initialize() fails (i.e. returns < 0),
caller may proceed further or exit the application/system.

2. Once step (1) is successful, create a global instance of class MotrLock,
call MotrLock::initialize() and pass it KV lock provider instance
created in step (1).

3. Just before acquiring a resource, call MotrLock::lock() on the global
instance of MotrLock.

4. After work with the resource is done, release the lock by calling MotrLock::
unlock() on the global instance of MotrLock.

Approach 2.
Steps:
1. Follow step (1) in Approach 1.
2. Call create_motr_Lock_instance(), passing it lock provider instance created
in step (1) above.
3. Call get_lock_instance() wherever lock is needed.

Note: Presently, locking framework supports EXCLUSIVE lock; it means
caller needs to set MotrLockType::EXCLUSIVE while calling  MotrLock::lock(),
and do not pass locker_id.
*/

#include "rgw_sal_motr.h"
#include "motr/sync/motr_sync.h"

class MotrLock : public MotrSync {
private:
  std::shared_ptr<MotrLockProvider> _lock_provider;

public:
  virtual void initialize(std::shared_ptr<MotrLockProvider> lock_provider) override;
  virtual int lock(const std::string& lock_name, MotrLockType lock_type,
                   utime_t lock_duration, const std::string& locker_id) override;
  virtual int unlock(const std::string& lock_name, MotrLockType lock_type,
                     const std::string& locker_id) override;
};

class MotrKVLockProvider : public MotrLockProvider {
private:
  const DoutPrefixProvider* _dpp;
  rgw::sal::MotrStore* _store;
  std::string _lock_index;
public:
  int initialize(const DoutPrefixProvider* dpp, rgw::sal::MotrStore* _s,
                 std::string& lock_index_name);
  virtual int read_lock(const std::string& lock_name,
                        motr_lock_info_t* lock_info) override;
  virtual int write_lock(const std::string& lock_name,
                         motr_lock_info_t* lock_info,
                         bool update = false) override;
  virtual int remove_lock(const std::string& lock_name,
                          const std::string& locker_id) override;
};

// Creates a global instance of MotrLock that can be used by caller
// This needs to be called by a single main thread of caller.
std::shared_ptr<MotrSync> g_motr_lock;
std::shared_ptr<MotrSync>& create_motr_Lock_instance(
    std::shared_ptr<MotrLockProvider> lock_provider) {
  static bool initialize = false;
  if (!initialize) {
    g_motr_lock = std::make_shared<MotrLock>();
    g_motr_lock->initialize(lock_provider);
    initialize = true;
    return g_motr_lock;
  }
}

std::shared_ptr<MotrSync>& get_lock_instance() {
  return g_motr_lock;
}

#endif
