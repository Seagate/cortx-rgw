// vim: ts=2 sw=2 expandtab ft=cpp

/*
 * Ceph - scalable distributed file system
 *
 * IO Policy implementation for Ceph Rados Gateway backend store(s)
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation. See file COPYING.
 *
 */

#pragma once

#include "rgw_sal_store.h"
#include "rgw_rados.h"
#include "rgw_notify.h"
#include "rgw_oidc_provider.h"
#include "rgw_role.h"
#include "rgw_multi.h"
#include "rgw_putobj_processor.h"

namespace rgw::sal {

class RGWStorePolicy
{
private:
    std::string name;
public:
    RGWStorePolicy()
    virtual ~RGWStorePolicy() = default;
    virtual rgw::sal::Store* get_bucket_store(CephContext* cct,
                                              std::string store_name);
    virtual rgw::sal::Store* get_user_store(CephContext* cct,
                                            std::string store_name);
        
}


}
