#include "motr_service.h"

int MOTRServices_Def::init(CephContext *cct, bool have_cache, bool raw, bool run_sync, optional_yield y, const DoutPrefixProvider *dpp)
{
  finisher = std::make_unique<RGWSI_Finisher>(cct);
  cls = std::make_unique<RGWSI_Cls>(cct);
  config_key_rados = std::make_unique<RGWSI_ConfigKey_RADOS>(cct);
  mdlog = std::make_unique<RGWSI_MDLog>(cct, run_sync);
  meta = std::make_unique<RGWSI_Meta>(cct);
  meta_be_sobj = std::make_unique<RGWSI_MetaBackend_SObj>(cct);
  meta_be_otp = std::make_unique<RGWSI_MetaBackend_OTP>(cct);
  notify = std::make_unique<RGWSI_Notify>(cct);
  otp = std::make_unique<RGWSI_OTP>(cct);
  rados = std::make_unique<RGWSI_RADOS>(cct);
  zone = std::make_unique<RGWSI_Zone>(cct);
  zone_utils = std::make_unique<RGWSI_ZoneUtils>(cct);
  quota = std::make_unique<RGWSI_Quota>(cct);
  sync_modules = std::make_unique<RGWSI_SyncModules>(cct);
  sysobj = std::make_unique<RGWSI_SysObj>(cct);
  sysobj_core = std::make_unique<MOTRSI_SysObj_Core>(cct);
  user_rados = std::make_unique<RGWSI_User_RADOS>(cct);

  if (have_cache) {
    sysobj_cache = std::make_unique<RGWSI_SysObj_Cache>(dpp, cct);
  }

  // bucket_sobj = std::make_unique<RGWSI_Bucket_SObj>(cct);
  // bucket_sync_sobj = std::make_unique<RGWSI_Bucket_Sync_SObj>(cct);
  // bi_rados = std::make_unique<RGWSI_BucketIndex_RADOS>(cct);
  // bilog_rados = std::make_unique<RGWSI_BILog_RADOS>(cct);
  // datalog_rados = std::make_unique<RGWDataChangesLog>(cct);

  finisher->init();
  // bi_rados->init(zone.get(), rados.get(), bilog_rados.get(), datalog_rados.get());
  // bilog_rados->init(bi_rados.get());
  // bucket_sobj->init(zone.get(), sysobj.get(), sysobj_cache.get(),
  //                   bi_rados.get(), meta.get(), meta_be_sobj.get(),
  //                   sync_modules.get(), bucket_sync_sobj.get());
  // bucket_sync_sobj->init(zone.get(),
  //                        sysobj.get(),
  //                        sysobj_cache.get(),
  //                        bucket_sobj.get());
  cls->init(zone.get(), rados.get());
  config_key_rados->init(rados.get());
  mdlog->init(rados.get(), zone.get(), sysobj.get(), cls.get());
  // meta->init(sysobj.get(), mdlog.get(), meta_bes);
  meta_be_sobj->init(sysobj.get(), mdlog.get());
  meta_be_otp->init(sysobj.get(), mdlog.get(), cls.get());
  notify->init(zone.get(), rados.get(), finisher.get());
  otp->init(zone.get(), meta.get(), meta_be_otp.get());
  rados->init();
  zone->init(sysobj.get(), rados.get(), sync_modules.get(), bucket_sync_sobj.get());
  zone_utils->init(rados.get(), zone.get());
  quota->init(zone.get());
  sync_modules->init(zone.get());
  sysobj_core->core_init(rados.get(), zone.get());
  if (have_cache) {
    // sysobj_cache->init(rados.get(), zone.get(), notify.get());
    sysobj->init(rados.get(), sysobj_cache.get());
  } else {
    sysobj->init(rados.get(), sysobj_core.get());
  }
  user_rados->init(rados.get(), zone.get(), sysobj.get(), sysobj_cache.get(),
                   meta.get(), meta_be_sobj.get(), sync_modules.get());

  can_shutdown = true;

  int r = finisher->start(y, dpp);
  if (r < 0) {
    // ldpp_dout(dpp, 0) << "ERROR: failed to start finisher service (" << cpp_strerror(-r) << dendl;
    return r;
  }

  if (!raw) {
    r = notify->start(y, dpp);
    if (r < 0) {
      // ldpp_dout(dpp, 0) << "ERROR: failed to start notify service (" << cpp_strerror(-r) << dendl;
      return r;
    }
  }

  r = rados->start(y, dpp);
  if (r < 0) {
    // ldpp_dout(dpp, 0) << "ERROR: failed to start rados service (" << cpp_strerror(-r) << dendl;
    return r;
  }

  if (!raw) {
    r = zone->start(y, dpp);
    if (r < 0) {
      // ldpp_dout(dpp, 0) << "ERROR: failed to start zone service (" << cpp_strerror(-r) << dendl;
      return r;
    }

    // r = datalog_rados->start(dpp, &zone->get_zone(),
		// 	     zone->get_zone_params(),
		// 	     rados->get_rados_handle());
    // if (r < 0) {
    //   // ldpp_dout(dpp, 0) << "ERROR: failed to start datalog_rados service (" << cpp_strerror(-r) << dendl;
    //   return r;
    // }

    r = mdlog->start(y, dpp);
    if (r < 0) {
      // ldpp_dout(dpp, 0) << "ERROR: failed to start mdlog service (" << cpp_strerror(-r) << dendl;
      return r;
    }

    r = sync_modules->start(y, dpp);
    if (r < 0) {
      // ldpp_dout(dpp, 0) << "ERROR: failed to start sync modules service (" << cpp_strerror(-r) << dendl;
      return r;
    }
  }

  r = cls->start(y, dpp);
  if (r < 0) {
    // ldpp_dout(dpp, 0) << "ERROR: failed to start cls service (" << cpp_strerror(-r) << dendl;
    return r;
  }

  r = config_key_rados->start(y, dpp);
  if (r < 0) {
    // ldpp_dout(dpp, 0) << "ERROR: failed to start config_key service (" << cpp_strerror(-r) << dendl;
    return r;
  }

  r = zone_utils->start(y, dpp);
  if (r < 0) {
    // ldpp_dout(dpp, 0) << "ERROR: failed to start zone_utils service (" << cpp_strerror(-r) << dendl;
    return r;
  }

  r = quota->start(y, dpp);
  if (r < 0) {
    // ldpp_dout(dpp, 0) << "ERROR: failed to start quota service (" << cpp_strerror(-r) << dendl;
    return r;
  }

  r = sysobj_core->start(y, dpp);
  if (r < 0) {
    // ldpp_dout(dpp, 0) << "ERROR: failed to start sysobj_core service (" << cpp_strerror(-r) << dendl;
    return r;
  }

  if (have_cache) {
    r = sysobj_cache->start(y, dpp);
    if (r < 0) {
      // ldpp_dout(dpp, 0) << "ERROR: failed to start sysobj_cache service (" << cpp_strerror(-r) << dendl;
      return r;
    }
  }

  r = sysobj->start(y, dpp);
  if (r < 0) {
    // ldpp_dout(dpp, 0) << "ERROR: failed to start sysobj service (" << cpp_strerror(-r) << dendl;
    return r;
  }

  if (!raw) {
    r = meta_be_sobj->start(y, dpp);
    if (r < 0) {
      // ldpp_dout(dpp, 0) << "ERROR: failed to start meta_be_sobj service (" << cpp_strerror(-r) << dendl;
      return r;
    }

    r = meta->start(y, dpp);
    if (r < 0) {
      // ldpp_dout(dpp, 0) << "ERROR: failed to start meta service (" << cpp_strerror(-r) << dendl;
      return r;
    }

    r = bucket_sobj->start(y, dpp);
    if (r < 0) {
      // ldpp_dout(dpp, 0) << "ERROR: failed to start bucket service (" << cpp_strerror(-r) << dendl;
      return r;
    }

    r = bucket_sync_sobj->start(y, dpp);
    if (r < 0) {
      // ldpp_dout(dpp, 0) << "ERROR: failed to start bucket_sync service (" << cpp_strerror(-r) << dendl;
      return r;
    }

    r = user_rados->start(y, dpp);
    if (r < 0) {
      // ldpp_dout(dpp, 0) << "ERROR: failed to start user_rados service (" << cpp_strerror(-r) << dendl;
      return r;
    }

    r = otp->start(y, dpp);
    if (r < 0) {
      // ldpp_dout(dpp, 0) << "ERROR: failed to start otp service (" << cpp_strerror(-r) << dendl;
      return r;
    }
  }

  /* cache or core services will be started by sysobj */

  return  0;
}

int MOTRServices::do_init(CephContext *_cct, bool have_cache, bool raw, bool run_sync, optional_yield y, const DoutPrefixProvider* dpp)
{
  cct = _cct;

  int r = motr_svc.init(cct, have_cache, raw, run_sync, y, nullptr);
  if (r < 0) {
    return r;
  }

  return 0;
}