#ifndef CEPH_MOTR_SERVICE_H
#define CEPH_MOTR_SERVICE_H

#pragma once

#include "rgw/rgw_service.h"

#include "svc_sys_obj_core.h"
#include "svc_rados.h"
#include "svc_sync_modules.h"
#include "svc_sys_obj.h"
#include "svc_sys_obj_cache.h"
#include "svc_sys_obj_core_types.h"
#include "svc_finisher.h"
#include "svc_bucket_sync_sobj.h"
#include "svc_bucket_sobj.h"
#include "svc_bilog_rados.h"
#include "svc_cls.h"
#include "svc_config_key_rados.h"
#include "svc_mdlog.h"
#include "svc_meta.h"
#include "svc_meta_be_sobj.h"
#include "svc_meta_be_otp.h"
#include "svc_notify.h"
#include "svc_otp.h"
#include "svc_zone.h"
#include "svc_zone_utils.h"
#include "svc_quota.h"
#include "svc_user_rados.h"

class MOTRSI_SysObj_Core : public RGWSI_SysObj_Core {
  public: 
    MOTRSI_SysObj_Core(CephContext* cct) : RGWSI_SysObj_Core(cct) {}
};

struct MOTRServices_Def : public RGWServices_Def {
  int init(CephContext *cct, bool have_cache, bool raw_storage, bool run_sync, optional_yield y, const DoutPrefixProvider *dpp) override;
  ~MOTRServices_Def() {}
};

struct MOTRServices : public RGWServices
{
  MOTRServices_Def motr_svc;

  int do_init(CephContext *cct, bool have_cache, bool raw_storage, bool run_sync, optional_yield y, const DoutPrefixProvider* dpp) override;

  int init_svc(bool raw) { 
    if (raw)
      return init_raw(cct, false, null_yield, nullptr);
      
    return init(cct, false, false, null_yield, nullptr);
  }

  void shutdown() override {
    motr_svc.shutdown();
  }

  ~MOTRServices() {}
};

#endif