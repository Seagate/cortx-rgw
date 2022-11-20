// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=2 sw=2 expandtab ft=cpp

/*
 * Ceph - scalable distributed file system
 *
 * SAL implementation for hybrid backend stores.
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation. See file COPYING.
 *
 */

#pragma once

#include <uuid/uuid.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "rgw_multi.h"
#include "rgw_notify.h"
#include "rgw_oidc_provider.h"
#include "rgw_putobj_processor.h"
#include "rgw_rados.h"
#include "rgw_role.h"
#include "rgw_sal.h"

/** Temporary use OBJECT_SIZE_THRESH to define the object size below which the
 *  objects are placed in Daos store
 */
#define OBJECT_SIZE_THRESH  (1 * 1024 * 1024)

namespace rgw::sal {
class HybridStore;
#if 0
class HybridUser : public StoreUser {
    private:
        HybridStore *store;

    public:
        HybridUser(HybridStore *_st, const rgw_user &_u) : StoreUser(_u), store(_st)
        {
        }
        HybridUser(HybridStore *_st, const RGWUserInfo &_i) : StoreUser(_i), store(_st)
        {
        }
        HybridUser(HybridStore *_st) : store(_st)
        {
        }
        HybridUser(HybridUser &_o) = default;
        HybridUser(void)
        {
        }

        virtual std::unique_ptr<User> clone(void)override
        {
            return std::unique_ptr<User>(new HybridUser(*this));
        }
        int list_buckets(const DoutPrefixProvider *dpp, const std::string &marker, const std::string &end_marker,
                         uint64_t max, bool need_stats, BucketList &buckets, optional_yield y)override;
        virtual int create_bucket(const DoutPrefixProvider *dpp,
                                  const rgw_bucket &b,
                                  const std::string &zonegroup_id,
                                  rgw_placement_rule &placement_rule,
                                  std::string &swift_ver_location,
                                  const RGWQuotaInfo *pquota_info,
                                  const RGWAccessControlPolicy &policy,
                                  Attrs &attrs,
                                  RGWBucketInfo &info,
                                  obj_version &ep_objv,
                                  bool exclusive,
                                  bool obj_lock_enabled,
                                  bool *existed,
                                  req_info &req_info,
                                  std::unique_ptr<Bucket> *bucket,
                                  optional_yield y)override;
        virtual int read_attrs(const DoutPrefixProvider *dpp, optional_yield y)override;
        virtual int merge_and_store_attrs(const DoutPrefixProvider *dpp, Attrs &new_attrs, optional_yield y)override;
        virtual int read_stats(const DoutPrefixProvider *dpp,
                               optional_yield y, RGWStorageStats *stats,
                               ceph::real_time *last_stats_sync = nullptr,
                               ceph::real_time *last_stats_update = nullptr)override;
        virtual int read_stats_async(const DoutPrefixProvider *dpp, RGWGetUserStats_CB *cb)override;
        virtual int complete_flush_stats(const DoutPrefixProvider *dpp, optional_yield y)override;
        virtual int read_usage(const DoutPrefixProvider *dpp, uint64_t start_epoch, uint64_t end_epoch, uint32_t max_entries,
                               bool *is_truncated, RGWUsageIter &usage_iter,
                               std::map<rgw_user_bucket, rgw_usage_log_entry> &usage)override;
        virtual int trim_usage(const DoutPrefixProvider *dpp, uint64_t start_epoch, uint64_t end_epoch)override;

        virtual int load_user(const DoutPrefixProvider *dpp, optional_yield y)override;
        virtual int store_user(const DoutPrefixProvider *dpp, optional_yield y, bool exclusive, RGWUserInfo *old_info = nullptr)override;
        virtual int remove_user(const DoutPrefixProvider *dpp, optional_yield y)override;

        int create_user_info_idx(void);

        friend class HybridBucket;
    };

    class HybridBucket : public StoreBucket {
    private:
        HybridStore *store;
        RGWAccessControlPolicy acls;

        // RGWBucketInfo and other information that are shown when listing a bucket is
        // represented in struct HybridBucketInfo. The structure is encoded and stored
        // as the value of the global bucket instance index.
        // TODO: compare pros and cons of separating the bucket_attrs (ACLs, tag etc.)
        // into a different index.
        struct HybridBucketInfo {
            RGWBucketInfo info;

            obj_version bucket_version;
            ceph::real_time mtime;

            rgw::sal::Attrs bucket_attrs;

            void encode(bufferlist &bl) const
            {
                ENCODE_START(4, 4, bl);
                encode(info, bl);
                encode(bucket_version, bl);
                encode(mtime, bl);
                encode(bucket_attrs, bl); //rgw_cache.h example for a map
                ENCODE_FINISH(bl);
            }

            void decode(bufferlist::const_iterator &bl)
            {
                DECODE_START(4, bl);
                decode(info, bl);
                decode(bucket_version, bl);
                decode(mtime, bl);
                decode(bucket_attrs, bl);
                DECODE_FINISH(bl);
            }
        };
        WRITE_CLASS_ENCODER(HybridBucketInfo);

    public:
        HybridBucket(HybridStore *_st)
            : store(_st),
              acls()
        {
        }

        HybridBucket(HybridStore *_st, User *_u)
            : StoreBucket(_u),
              store(_st),
              acls()
        {
        }

        HybridBucket(HybridStore *_st, const rgw_bucket &_b)
            : StoreBucket(_b),
              store(_st),
              acls()
        {
        }

        HybridBucket(HybridStore *_st, const RGWBucketEnt &_e)
            : StoreBucket(_e),
              store(_st),
              acls()
        {
        }

        HybridBucket(HybridStore *_st, const RGWBucketInfo &_i)
            : StoreBucket(_i),
              store(_st),
              acls()
        {
        }

        HybridBucket(HybridStore *_st, const rgw_bucket &_b, User *_u)
            : StoreBucket(_b, _u),
              store(_st),
              acls()
        {
        }

        HybridBucket(HybridStore *_st, const RGWBucketEnt &_e, User *_u)
            : StoreBucket(_e, _u),
              store(_st),
              acls()
        {
        }

        HybridBucket(HybridStore *_st, const RGWBucketInfo &_i, User *_u)
            : StoreBucket(_i, _u),
              store(_st),
              acls()
        {
        }

        ~HybridBucket(void)
        {
        }

        virtual std::unique_ptr<Object> get_object(const rgw_obj_key &k)override;
        virtual int list(const DoutPrefixProvider *dpp, ListParams &, int, ListResults &, optional_yield y)override;
        virtual int remove_bucket(const DoutPrefixProvider *dpp, bool delete_children, bool forward_to_master, req_info *req_info, optional_yield y)override;
        virtual int remove_bucket_bypass_gc(int concurrent_max, bool
                                            keep_index_consistent,
                                            optional_yield y, const
                                            DoutPrefixProvider *dpp)override;
        virtual RGWAccessControlPolicy &get_acl(void)override
        {
            return acls;
        }
        virtual int set_acl(const DoutPrefixProvider *dpp, RGWAccessControlPolicy &acl, optional_yield y)override;
        virtual int load_bucket(const DoutPrefixProvider *dpp, optional_yield y, bool get_stats = false)override;
        int link_user(const DoutPrefixProvider *dpp, User *new_user, optional_yield y);
        int unlink_user(const DoutPrefixProvider *dpp, User *new_user, optional_yield y);
        int create_bucket_index(void);
        int create_multipart_indices(void);
        virtual int read_stats(const DoutPrefixProvider *dpp,
                               const bucket_index_layout_generation &idx_layout, int shard_id,
                               std::string *bucket_ver, std::string *master_ver,
                               std::map<RGWObjCategory, RGWStorageStats> &stats,
                               std::string *max_marker = nullptr,
                               bool *syncstopped = nullptr)override;
        virtual int read_stats_async(const DoutPrefixProvider *dpp,
                                     const bucket_index_layout_generation &idx_layout,
                                     int shard_id, RGWGetBucketStats_CB *ctx)override;
        virtual int sync_user_stats(const DoutPrefixProvider *dpp, optional_yield y)override;
        virtual int update_container_stats(const DoutPrefixProvider *dpp)override;
        virtual int check_bucket_shards(const DoutPrefixProvider *dpp)override;
        virtual int chown(const DoutPrefixProvider *dpp, User *new_user, User *old_user, optional_yield y, const std::string *marker = nullptr)override;
        virtual int put_info(const DoutPrefixProvider *dpp, bool exclusive, ceph::real_time mtime)override;
        virtual bool is_owner(User *user)override;
        virtual int check_empty(const DoutPrefixProvider *dpp, optional_yield y)override;
        virtual int check_quota(const DoutPrefixProvider *dpp, RGWQuota &quota, uint64_t obj_size, optional_yield y, bool check_size_only = false)override;
        virtual int merge_and_store_attrs(const DoutPrefixProvider *dpp, Attrs &attrs, optional_yield y)override;
        virtual int try_refresh_info(const DoutPrefixProvider *dpp, ceph::real_time *pmtime)override;
        virtual int read_usage(const DoutPrefixProvider *dpp, uint64_t start_epoch, uint64_t end_epoch, uint32_t max_entries,
                               bool *is_truncated, RGWUsageIter &usage_iter,
                               std::map<rgw_user_bucket, rgw_usage_log_entry> &usage)override;
        virtual int trim_usage(const DoutPrefixProvider *dpp, uint64_t start_epoch, uint64_t end_epoch)override;
        virtual int remove_objs_from_index(const DoutPrefixProvider *dpp, std::list<rgw_obj_index_key> &objs_to_unlink)override;
        virtual int check_index(const DoutPrefixProvider *dpp, std::map<RGWObjCategory, RGWStorageStats> &existing_stats, std::map<RGWObjCategory, RGWStorageStats> &calculated_stats)override;
        virtual int rebuild_index(const DoutPrefixProvider *dpp)override;
        virtual int set_tag_timeout(const DoutPrefixProvider *dpp, uint64_t timeout)override;
        virtual int purge_instance(const DoutPrefixProvider *dpp)override;
        virtual std::unique_ptr<Bucket> clone(void)override
        {
            return std::make_unique<HybridBucket>(*this);
        }
        virtual std::unique_ptr<MultipartUpload> get_multipart_upload(const std::string &oid,
                                                                      std::optional<std::string> upload_id = std::nullopt,
                                                                      ACLOwner owner = {
        }, ceph::real_time mtime = real_clock::now())override;
        virtual int list_multiparts(const DoutPrefixProvider *dpp,
                                    const std::string &prefix,
                                    std::string &marker,
                                    const std::string &delim,
                                    const int &max_uploads,
                                    std::vector<std::unique_ptr<MultipartUpload>>& uploads,
                                                std::map<std::string, bool> *common_prefixes,
                                                bool *is_truncated) override;
        virtual int abort_multiparts(const DoutPrefixProvider *dpp, CephContext *cct)override;

        friend class HybridStore;
    };

    class HybridPlacementTier : public StorePlacementTier {
        HybridStore *store;
        RGWZoneGroupPlacementTier tier;
    public:
        HybridPlacementTier(HybridStore *_store, const RGWZoneGroupPlacementTier &_tier) : store(_store), tier(_tier)
        {
        }
        virtual ~HybridPlacementTier(void) = default;

        virtual const std::string &get_tier_type(void)
        {
            return tier.tier_type;
        }
        virtual const std::string &get_storage_class(void)
        {
            return tier.storage_class;
        }
        virtual bool retain_head_object(void)
        {
            return tier.retain_head_object;
        }
        RGWZoneGroupPlacementTier &get_rt(void)
        {
            return tier;
        }
    };

    class HybridZoneGroup : public StoreZoneGroup {
        HybridStore *store;
        const RGWZoneGroup group;
        std::string empty;
    public:
        HybridZoneGroup(HybridStore *_store) : store(_store), group()
        {
        }
        HybridZoneGroup(HybridStore *_store, const RGWZoneGroup &_group) : store(_store), group(_group)
        {
        }
        virtual ~HybridZoneGroup(void) = default;

        virtual const std::string &get_id(void) const override
        {
            return group.get_id();
        };
        virtual const std::string &get_name(void) const override
        {
            return group.get_name();
        };
        virtual int equals(const std::string &other_zonegroup) const override
        {
            return group.equals(other_zonegroup);
        };
        /** Get the endpoint from zonegroup, or from master zone if not set */
        virtual const std::string &get_endpoint(void) const override;
        virtual bool placement_target_exists(std::string &target) const override;
        virtual bool is_master_zonegroup(void) const override
        {
            return group.is_master_zonegroup();
        };
        virtual const std::string &get_api_name(void) const override
        {
            return group.api_name;
        };
        virtual int get_placement_target_names(std::set<std::string> &names) const override;
        virtual const std::string &get_default_placement_name(void) const override
        {
            return group.default_placement.name;
        };
        virtual int get_hostnames(std::list<std::string> &names) const override
        {
            names = group.hostnames;
            return 0;
        };
        virtual int get_s3website_hostnames(std::list<std::string> &names) const override
        {
            names = group.hostnames_s3website;
            return 0;
        };
        virtual int get_zone_count(void) const override
        {
            return group.zones.size();
        }
        virtual int get_placement_tier(const rgw_placement_rule &rule, std::unique_ptr<PlacementTier> *tier);
        const RGWZoneGroup &get_group(void)
        {
            return group;
        }
        virtual std::unique_ptr<ZoneGroup> clone(void)override
        {
            return std::make_unique<HybridZoneGroup>(store, group);
        }
    };

    class HybridZone : public StoreZone {
    protected:
        HybridStore *store;
        RGWRealm *realm{nullptr};
        HybridZoneGroup zonegroup;
        RGWZone *zone_public_config{nullptr}; /* external zone params, e.g., entrypoints, log flags, etc. */
        RGWZoneParams *zone_params{nullptr}; /* internal zone params, e.g., rados pools */
        RGWPeriod *current_period{nullptr};
        rgw_zone_id cur_zone_id;

    public:
        HybridZone(HybridStore *_store) : store(_store), zonegroup(_store)
        {
            realm = new RGWRealm();
            zone_public_config = new RGWZone();
            zone_params = new RGWZoneParams();
            current_period = new RGWPeriod();
            cur_zone_id = rgw_zone_id(zone_params->get_id());

            // XXX: only default and STANDARD supported for now
            RGWZonePlacementInfo  info;
            RGWZoneStorageClasses sc;

            sc.set_storage_class("STANDARD", nullptr, nullptr);
            info.storage_classes = sc;
            zone_params->placement_pools["default"] = info;
        }
        HybridZone(HybridStore *_store, HybridZoneGroup _zg) : store(_store), zonegroup(_zg)
        {
            realm = new RGWRealm();
            zone_public_config = new RGWZone();
            zone_params = new RGWZoneParams();
            current_period = new RGWPeriod();
            cur_zone_id = rgw_zone_id(zone_params->get_id());

            // XXX: only default and STANDARD supported for now
            RGWZonePlacementInfo  info;
            RGWZoneStorageClasses sc;

            sc.set_storage_class("STANDARD", nullptr, nullptr);
            info.storage_classes = sc;
            zone_params->placement_pools["default"] = info;
        }
        ~HybridZone(void) = default;

        virtual std::unique_ptr<Zone> clone(void)override
        {
            return std::make_unique<HybridZone>(store);
        }
        virtual ZoneGroup &get_zonegroup(void)override;
        virtual int get_zonegroup(const std::string &id, std::unique_ptr<ZoneGroup> *zonegroup)override;
        virtual const rgw_zone_id &get_id(void)override;
        virtual const std::string &get_name(void) const override;
        virtual bool is_writeable(void)override;
        virtual bool get_redirect_endpoint(std::string *endpoint)override;
        virtual bool has_zonegroup_api(const std::string &api) const override;
        virtual const std::string &get_current_period_id(void)override;
        virtual const RGWAccessKey &get_system_key(void)
        {
            return zone_params->system_key;
        }
        virtual const std::string &get_realm_name(void)
        {
            return realm->get_name();
        }
        virtual const std::string &get_realm_id(void)
        {
            return realm->get_id();
        }
        virtual const std::string_view get_tier_type(void)
        {
            return "rgw";
        }
        friend class HybridStore;
    };

    class HybridLuaManager : public StoreLuaManager {
        HybridStore *store;

    public:
        HybridLuaManager(HybridStore *_s) : store(_s)
        {
        }
        virtual ~HybridLuaManager(void) = default;

        /** Get a script named with the given key from the backing store */
        virtual int get_script(const DoutPrefixProvider *dpp, optional_yield y, const std::string &key, std::string &script)override;
        /** Put a script named with the given key to the backing store */
        virtual int put_script(const DoutPrefixProvider *dpp, optional_yield y, const std::string &key, const std::string &script)override;
        /** Delete a script named with the given key from the backing store */
        virtual int del_script(const DoutPrefixProvider *dpp, optional_yield y, const std::string &key)override;
        /** Add a lua package */
        virtual int add_package(const DoutPrefixProvider *dpp, optional_yield y, const std::string &package_name)override;
        /** Remove a lua package */
        virtual int remove_package(const DoutPrefixProvider *dpp, optional_yield y, const std::string &package_name)override;
        /** List lua packages */
        virtual int list_packages(const DoutPrefixProvider *dpp, optional_yield y, rgw::lua::packages_t &packages)override;
    };

    class HybridOIDCProvider : public RGWOIDCProvider {
        HybridStore *store;
    public:
        HybridOIDCProvider(HybridStore *_store) : store(_store)
        {
        }
        ~HybridOIDCProvider(void) = default;

        virtual int store_url(const DoutPrefixProvider *dpp, const std::string &url, bool exclusive, optional_yield y)override
        {
            return 0;
        }
        virtual int read_url(const DoutPrefixProvider *dpp, const std::string &url, const std::string &tenant)override
        {
            return 0;
        }
        virtual int delete_obj(const DoutPrefixProvider *dpp, optional_yield y)override
        {
            return 0;
        }

        void encode(bufferlist &bl) const
        {
            RGWOIDCProvider::encode(bl);
        }
        void decode(bufferlist::const_iterator &bl)
        {
            RGWOIDCProvider::decode(bl);
        }
    };
#endif

//    class HybridObject : public rgw::sal::DaosObject, public rgw::sal::MotrObject {
    class HybridObject : public rgw::sal::MotrObject {
    private:
        HybridStore *store;

        /** Only one of the below should be SET after the Object has been
         *  initialized. If none or both are set then code needs to be
         *  debugged ;)
         */
        bool         onCORTX;
        bool         onDAOS;

    public:

//        struct HybridReadOp : public DaosReadOp, public MotrReadOp {
        struct HybridReadOp : public MotrReadOp {
        private:
            // The set of part objects if the source is
            // a multipart uploaded object.
            std::map<int, std::unique_ptr<HybridObject>> part_objs;

                 public:
                     HybridReadOp(HybridObject *_source) : DaosReadOp(_source), MotrReadOp(_source) {}

                     virtual int prepare(optional_yield y, const DoutPrefixProvider *dpp)override
                     {
                         ceph_assert(source->onCORTX ^ source->onDAOS);

                         return (source->onCORTX) ?
                                    MotrReadOp::prepare(y, dpp) :
                                    DaosReadOp::prepare(y, dpp);
                     }
                     virtual int read(int64_t off, int64_t end, bufferlist &bl, optional_yield y, const DoutPrefixProvider *dpp)override
                     {
                         ceph_assert(source->onCORTX ^ source->onDAOS);

                         return (source->onCORTX) ?
                                    MotrReadOp::read( off, end, bl, y, dpp) :
                                    DaosReadOp::read( off, end, bl, y, dpp);
                     }
                     virtual int iterate(const DoutPrefixProvider *dpp, int64_t off, int64_t end, RGWGetDataCB *cb, optional_yield y)override
                     {
                         ceph_assert(source->onCORTX ^ source->onDAOS);

                         return (source->onCORTX) ?
                                    MotrReadOp::iterate( dpp, off, end, cb, y) :
                                    DaosReadOp::iterate( dpp, off, end, cb, y);
                     }
                     virtual int get_attr(const DoutPrefixProvider *dpp, const char *name, bufferlist &dest, optional_yield y)override
                     {
                         ceph_assert(source->onCORTX ^ source->onDAOS);

                         return (source->onCORTX) ?
                                    MotrReadOp::get_attr(dpp, name, dest, y) :
                                    DaosReadOp::get_attr(dpp, name, dest, y);
                     }
        };

//        struct HybridDeleteOp : public DaosDeleteOp, public MotrDeleteOp {
        struct HybridDeleteOp : public MotrDeleteOp {
        private:
            HybridObject *source;

        public:
            HybridDeleteOp(HybridObject *_source);

            virtual int delete_obj(const DoutPrefixProvider *dpp, optional_yield y)override
            {
                ceph_assert(source->onCORTX ^ source->onDAOS);

                return (source->onCORTX) ?
                       MotrReadOp::delete_obj(dpp, y) :
                       DaosReadOp::delete_obj(dpp, y);
            }
        };

        HybridObject(void) = default;

        HybridObject(HybridStore *_st, const rgw_obj_key &_k)
            : DaosObject(_st, _k), MotrObject(_st, _k)
        {
        }
        HybridObject(HybridStore *_st, const rgw_obj_key &_k, Bucket *_b)
            : DaosObject(_st, _k, _b), MotrObject(_st, _k, _b)
        {
        }

        HybridObject(HybridObject &_o) = default;

        virtual ~HybridObject(void) = default;

        virtual int delete_object(const DoutPrefixProvider *dpp,
                                  RGWObjectCtx *obj_ctx,
                                  optional_yield y,
                                  bool prevent_versioning = false)override
        {
            ceph_assert(onCORTX ^ onDAOS);
            return (onCORTX) ?
                   MotrObject::delete_object(dpp, ctx, y, prevent_versioning) :
                   DaosReadOp::delete_object(dpp, ctx, y, prevent_versioning);
        }
        virtual int delete_obj_aio(const DoutPrefixProvider *dpp, RGWObjState *astate, Completions *aio,
                                   bool keep_index_consistent, optional_yield y)override
        {
            ceph_assert(onCORTX ^ onDAOS);
            return (onCORTX) ?
                   MotrObject::delete_obj_aio( dpp, astate, aio, keep_index_consistent, y) :
                   DaosReadOp::delete_obj_aio(dpp, astate, aio, keep_index_consistent, y);
        }
        virtual int copy_object(RGWObjectCtx &obj_ctx, User *user,
                                req_info *info, const rgw_zone_id &source_zone,
                                rgw::sal::Object *dest_object, rgw::sal::Bucket *dest_bucket,
                                rgw::sal::Bucket *src_bucket,
                                const rgw_placement_rule &dest_placement,
                                ceph::real_time *src_mtime, ceph::real_time *mtime,
                                const ceph::real_time *mod_ptr, const ceph::real_time *unmod_ptr,
                                bool high_precision_time,
                                const char *if_match, const char *if_nomatch,
                                AttrsMod attrs_mod, bool copy_if_newer, Attrs &attrs,
                                RGWObjCategory category, uint64_t olh_epoch,
                                boost::optional<ceph::real_time> delete_at,
                                std::string *version_id, std::string *tag, std::string *etag,
                                void (*progress_cb)(off_t, void *), void *progress_data,
                                const DoutPrefixProvider *dpp, optional_yield y)override
        {
            ceph_assert(onCORTX ^ onDAOS);
            return (onCORTX) ?
                   MotrObject::copy_object(obj_ctx, user, info, source_zone, dest_object,
                                            dest_bucket, src_bucket, dest_placement,
                                            src_mtime, mtime, mod_ptr, unmod_ptr,
                                            high_precision_time, if_match, if_nomatch,
                                            attrs_mod, copy_if_newer, attrs, category,
                                            olh_epoch, delete_at, version_id, tag,
                                            etag, progress_cb, progress_data, dpp, y) :
                   DaosReadOp::copy_object(obj_ctx, user, info, source_zone, dest_object,
                                           dest_bucket, src_bucket, dest_placement,
                                           src_mtime, mtime, mod_ptr, unmod_ptr,
                                           high_precision_time, if_match, if_nomatch,
                                           attrs_mod, copy_if_newer, attrs, category,
                                           olh_epoch, delete_at, version_id, tag,
                                           etag, progress_cb, progress_data, dpp, y);
        }
        virtual RGWAccessControlPolicy &get_acl(void)override
        {
            ceph_assert(onCORTX ^ onDAOS);
            return (onCORTX) ? MotrObject::get_acl() : DaosObject::get_acl();
        }
        virtual int set_acl(const RGWAccessControlPolicy &acl)override
        {
            ceph_assert(onCORTX ^ onDAOS);
            return (onCORTX) ? MotrObject::set_acl(acl) : DaosObject::set_acl(acl);
        }
        virtual int get_obj_state(const DoutPrefixProvider *dpp, RGWObjectCtx *rctx, RGWObjState **state, optional_yield y, bool follow_olh = true)override
        {
            ceph_assert(onCORTX ^ onDAOS);
            return (onCORTX) ? MotrObject::get_obj_state( dpp, rctx, state, y, follow_olh) :
                               DaosObject::get_obj_state(dpp, state, y, follow_olh);
        }
        virtual int set_obj_attrs(const DoutPrefixProvider *dpp, RGWObjectCtx *rctx, Attrs *setattrs, Attrs *delattrs, optional_yield y, rgw_obj *target_obj = NULL)override
        {
            ceph_assert(onCORTX ^ onDAOS);
            return (onCORTX) ? MotrObject::set_obj_attrs( dpp, rctx, setattrs, delattrs, y, target_obj) :
                               DaosObject::set_obj_attrs(dpp, rctx, setattrs, delattrs, y, target_obj);
        }
        virtual int get_obj_attrs(RGWObjectCtx *rctx, optional_yield y, const DoutPrefixProvider *dpp, rgw_obj *target_obj = NULL)override
        {
            ceph_assert(onCORTX ^ onDAOS);
            return (onCORTX) ? MotrObject::get_obj_attrs(rctx, y, dpp, target_obj) :
                               DaosObject::get_obj_attrs(rctx, y, dpp, target_obj);
        }
        virtual int modify_obj_attrs(RGWObjectCtx *rctx, const char *attr_name, bufferlist &attr_val, optional_yield y, const DoutPrefixProvider *dpp)override
        {
            ceph_assert(onCORTX ^ onDAOS);
            return (onCORTX) ? MotrObject::modify_obj_attrs(rctx, attr_name, attr_val, y, dpp) :
                               DaosObject::modify_obj_attrs(rctx, attr_name, attr_val, y, dpp);
        }
        virtual int delete_obj_attrs(const DoutPrefixProvider *dpp, RGWObjectCtx *rctx, const char *attr_name, optional_yield y)override
        {
            ceph_assert(onCORTX ^ onDAOS);
            return (onCORTX) ? MotrObject::delete_obj_attrs(dpp, rctx, attr_name, y) :
                               DaosObject::delete_obj_attrs(dpp, rctx, attr_name, y);
        }
        virtual bool is_expired(void)override
        {
            ceph_assert(onCORTX ^ onDAOS);
            return (onCORTX) ? MotrObject::is_expired() :
                               DaosObject::is_expired();
        }
        virtual void gen_rand_obj_instance_name(void)override
        {
            ceph_assert(onCORTX ^ onDAOS);
            return (onCORTX) ? MotrObject::gen_rand_obj_instance_name() :
                               DaosObject::gen_rand_obj_instance_name();
        }
        virtual std::unique_ptr<Object> clone(void)override
        {
            return std::unique_ptr<Object>(new HybridObject(*this));
        }
        virtual MPSerializer* get_serializer(const DoutPrefixProvider *dpp, const std::string &lock_name)override
        {
            ceph_assert(onCORTX ^ onDAOS);
            return (onCORTX) ? MotrObject::get_serializer( dpp, lock_name) :
                               DaosObject::get_serializer(dpp, lock_name);
        }
        virtual int transition(RGWObjectCtx &rctx, Bucket *bucket,
                               const rgw_placement_rule &placement_rule,
                               const real_time &mtime,
                               uint64_t olh_epoch,
                               const DoutPrefixProvider *dpp,
                               optional_yield y)override
        {
            return DaosObject::transition(rctx, bucket, placement_rule, mtime, olh_epoch, dpp, y);
        }
        virtual bool placement_rules_match(rgw_placement_rule &r1, rgw_placement_rule &r2)override
        {
            return DaosObject::placement_rules_match(r1, r2);
        }
        virtual int dump_obj_layout(const DoutPrefixProvider *dpp, optional_yield y, Formatter *f, RGWObjectCtx *obj_ctx)override
        {
            return DaosObject::dump_obj_layout(dpp, y, f, obj_ctx);
        }

        /* Swift versioning */
        virtual int swift_versioning_restore(RGWObjectCtx *obj_ctx, bool &restored,
                                             const DoutPrefixProvider *dpp)override
        {
            return DaosObject::swift_versioning_restore(obj_ctx, restored, dpp);
        }
        virtual int swift_versioning_copy(RGWObjectCtx *obj_ctx, const DoutPrefixProvider *dpp,
                                          optional_yield y)override
        {
            return DaosObject::swift_versioning_copy(obj_ctx, dpp, y);
        }

        /* OPs */
        virtual std::unique_ptr<ReadOp> get_read_op(RGWObjectCtx *ctx)override
        {
            return std::make_unique<HybridObject::HybridReadOp>(this, ctx);
        }
        virtual std::unique_ptr<DeleteOp> get_delete_op(RGWObjectCtx *ctx)override
        {
            return std::make_unique<HybridObject::HybridDeleteOp>(this, ctx);
        }

        /* OMAP */
        virtual int omap_get_vals(const DoutPrefixProvider *dpp, const std::string &marker, uint64_t count,
                                  std::map<std::string, bufferlist> *m,
                                  bool *pmore, optional_yield y)override
        {
            return DaosObject::omap_get_vals(dpp, marker, count, m, pmore, y);
        }
        virtual int omap_get_all(const DoutPrefixProvider *dpp, std::map<std::string, bufferlist> *m,
                                 optional_yield y)override
        {
            return DaosObject::omap_get_all(dpp, m, y);
        }
        virtual int omap_get_vals_by_keys(const DoutPrefixProvider *dpp, const std::string &oid,
                                          const std::set<std::string> &keys,
                                          Attrs *vals)override
        {
            return DaosObject::omap_get_vals_by_keys(dpp, oid, keys, vals);
        }
        virtual int omap_set_val_by_key(const DoutPrefixProvider *dpp, const std::string &key, bufferlist &val,
                                        bool must_exist, optional_yield y)override
        {
            return DaosObject::omap_set_val_by_key(dpp, key, val, must_exist, y);
        }
    private:
        //int read_attrs(const DoutPrefixProvider* dpp, Hybrid::Object::Read &read_op, optional_yield y, rgw_obj* target_obj = nullptr);

    public:
        bool is_opened(void)
        {
            ceph_assert(onCORTX ^ onDAOS);
            return (onCORTX) ? MotrObject::is_opened() :
                               DaosObject::is_opened();
        }
    };

#if 0 // We probably do not need an implementation of this in Hybrid case. Need to understand the use of serializer to confirm it.
    // A placeholder locking class for multipart upload.
    // TODO: implement it using Hybrid object locks.
    class MPHybridSerializer : public MPMotrSerializer, public MPDaosSerializer {

    public:
        MPHybridSerializer(const DoutPrefixProvider *dpp, HybridStore *store, HybridObject *obj, const std::string &lock_name)
        {
        }

        virtual int try_lock(const DoutPrefixProvider *dpp, utime_t dur, optional_yield y)override
        {
            return 0;
        }
        virtual int unlock(void)override
        {
            return 0;
        }
    };
#endif

    class HybridAtomicWriter : public StoreWriter {
    protected:
        rgw::sal::HybridStore *store;
        const rgw_user &owner;
        const rgw_placement_rule *ptail_placement_rule;
        uint64_t olh_epoch;
        const std::string &unique_tag;
        HybridObject obj;
        uint64_t total_data_size; // for total data being uploaded
        bufferlist acc_data;  // accumulated data
        uint64_t acc_off; // accumulated data offset

        struct m0_bufvec buf;
        struct m0_bufvec attr;
        struct m0_indexvec ext;

    public:
        HybridAtomicWriter(const DoutPrefixProvider *dpp,
                           optional_yield y,
                           std::unique_ptr<rgw::sal::Object> _head_obj,
                           HybridStore *_store,
                           const rgw_user &_owner,
                           const rgw_placement_rule *_ptail_placement_rule,
                           uint64_t _olh_epoch,
                           const std::string &_unique_tag);
        ~HybridAtomicWriter(void) = default;

        // prepare to start processing object data
        virtual int prepare(optional_yield y)override;

        // Process a bufferlist
        virtual int process(bufferlist &&data, uint64_t offset)override;

        int write(void);

        // complete the operation and make its result visible to clients
        virtual int complete(size_t accounted_size, const std::string &etag,
                             ceph::real_time *mtime, ceph::real_time set_mtime,
                             std::map<std::string, bufferlist> &attrs,
                             ceph::real_time delete_at,
                             const char *if_match, const char *if_nomatch,
                             const std::string *user_data,
                             rgw_zone_set *zones_trace, bool *canceled,
                             optional_yield y)override;

        unsigned populate_bvec(unsigned len, bufferlist::iterator &bi);
        void cleanup(void);
    };

    class HybridMultipartWriter : public StoreWriter {
    protected:
        rgw::sal::HybridStore *store;

        // Head object.
        std::unique_ptr<rgw::sal::Object> head_obj;

        // Part parameters.
        const uint64_t part_num;
        const std::string part_num_str;
        std::unique_ptr<HybridObject> part_obj;
        uint64_t actual_part_size = 0;

    public:
        HybridMultipartWriter(const DoutPrefixProvider *dpp,
                              optional_yield y, MultipartUpload *upload,
                              std::unique_ptr<rgw::sal::Object> _head_obj,
                              HybridStore *_store,
                              const rgw_user &owner,
                              const rgw_placement_rule *ptail_placement_rule,
                              uint64_t _part_num, const std::string &part_num_str) :
            StoreWriter(dpp, y), store(_store), head_obj(std::move(_head_obj)),
            part_num(_part_num), part_num_str(part_num_str)
        {
        }
        ~HybridMultipartWriter(void) = default;

        // prepare to start processing object data
        virtual int prepare(optional_yield y)override;

        // Process a bufferlist
        virtual int process(bufferlist &&data, uint64_t offset)override;

        // complete the operation and make its result visible to clients
        virtual int complete(size_t accounted_size, const std::string &etag,
                             ceph::real_time *mtime, ceph::real_time set_mtime,
                             std::map<std::string, bufferlist> &attrs,
                             ceph::real_time delete_at,
                             const char *if_match, const char *if_nomatch,
                             const std::string *user_data,
                             rgw_zone_set *zones_trace, bool *canceled,
                             optional_yield y)override;
    };

    class HybridMultipartPart : public StoreMultipartPart {
    protected:
        RGWUploadPartInfo info;

    public:
        HybridObject::Meta meta;

        HybridMultipartPart(RGWUploadPartInfo _info, HybridObject::Meta _meta) :
            info(_info), meta(_meta)
        {
        }
        virtual ~HybridMultipartPart(void) = default;

        virtual uint32_t get_num(void)
        {
            return info.num;
        }
        virtual uint64_t get_size(void)
        {
            return info.accounted_size;
        }
        virtual const std::string &get_etag(void)
        {
            return info.etag;
        }
        virtual ceph::real_time &get_mtime(void)
        {
            return info.modified;
        }

        RGWObjManifest &get_manifest(void)
        {
            return info.manifest;
        }

        friend class HybridMultipartUpload;
    };

    class HybridMultipartUpload : public StoreMultipartUpload {
        HybridStore *store;
        RGWMPObj mp_obj;
        ACLOwner owner;
        ceph::real_time mtime;
        rgw_placement_rule placement;
        RGWObjManifest manifest;

    public:
        HybridMultipartUpload(HybridStore *_store, Bucket *_bucket, const std::string &oid,
                              std::optional<std::string> upload_id, ACLOwner _owner, ceph::real_time _mtime) :
            StoreMultipartUpload(_bucket), store(_store), mp_obj(oid, upload_id), owner(_owner), mtime(_mtime)
        {
        }
        virtual ~HybridMultipartUpload(void) = default;

        virtual const std::string &get_meta(void) const
        {
            return mp_obj.get_meta();
        }
        virtual const std::string &get_key(void) const
        {
            return mp_obj.get_key();
        }
        virtual const std::string &get_upload_id(void) const
        {
            return mp_obj.get_upload_id();
        }
        virtual const ACLOwner &get_owner(void) const override
        {
            return owner;
        }
        virtual ceph::real_time &get_mtime(void)
        {
            return mtime;
        }
        virtual std::unique_ptr<rgw::sal::Object> get_meta_obj(void)override;
        virtual int init(const DoutPrefixProvider *dpp, optional_yield y, ACLOwner &owner, rgw_placement_rule &dest_placement, rgw::sal::Attrs &attrs)override;
        virtual int list_parts(const DoutPrefixProvider *dpp, CephContext *cct,
                               int num_parts, int marker,
                               int *next_marker, bool *truncated,
                               bool assume_unsorted = false)override;
        virtual int abort(const DoutPrefixProvider *dpp, CephContext *cct)override;
        virtual int complete(const DoutPrefixProvider *dpp,
                             optional_yield y, CephContext *cct,
                             std::map<int, std::string> &part_etags,
                             std::list<rgw_obj_index_key> &remove_objs,
                             uint64_t &accounted_size, bool &compressed,
                             RGWCompressionInfo &cs_info, off_t &off,
                             std::string &tag, ACLOwner &owner,
                             uint64_t olh_epoch,
                             rgw::sal::Object *target_obj)override;
        virtual int get_info(const DoutPrefixProvider *dpp, optional_yield y, rgw_placement_rule **rule, rgw::sal::Attrs *attrs = nullptr)override;
        virtual std::unique_ptr<Writer> get_writer(const DoutPrefixProvider *dpp,
                                                   optional_yield y,
                                                   std::unique_ptr<rgw::sal::Object> _head_obj,
                                                   const rgw_user &owner,
                                                   const rgw_placement_rule *ptail_placement_rule,
                                                   uint64_t part_num,
                                                   const std::string &part_num_str)override;
        int delete_parts(const DoutPrefixProvider *dpp);
    };

    class HybridStore : public MotrStore, public DaosStore {
    private:

    public:

        HybridStore(CephContext *c) {}
        ~HybridStore(void) {}

        virtual int initialize(CephContext *cct,
                               const DoutPrefixProvider *dpp)override
        {
            return DaosStore::initialize(cct, dpp);
        }
        virtual const std::string get_name(void) const override
        {
            return "hybrid";
        }

        virtual std::unique_ptr<User> get_user(const rgw_user &u)override
        {
            return DaosStore::get_user(u);
        }
        virtual std::string get_cluster_id(const DoutPrefixProvider *dpp,
                                           optional_yield y)override
        {
            return DaosStore::get_cluster_id(dpp, y);
        }
        virtual int get_user_by_access_key(const DoutPrefixProvider *dpp,
                                           const std::string &key,
                                           optional_yield y,
                                           std::unique_ptr<User> *user)override
        {
            return DaosStore::get_user_by_access_key(dpp, key, y, user);
        }
        virtual int get_user_by_email(const DoutPrefixProvider *dpp,
                                      const std::string &email,
                                      optional_yield y,
                                      std::unique_ptr<User> *user)override
        {
            return DaosStore::get_user_by_email(dpp, email, y, user);
        }
        virtual int get_user_by_swift(const DoutPrefixProvider *dpp,
                                      const std::string &user_str,
                                      optional_yield y,
                                      std::unique_ptr<User> *user)override
        {
            return DaosStore::get_user_by_swift(dpp, user_str, y, user);
        }
        virtual std::unique_ptr<Object> get_object(const rgw_obj_key &k)override
        {
            return DaosStore::get_object(k);
        }
        virtual int get_bucket(const DoutPrefixProvider *dpp,
                               User *u,
                               const rgw_bucket &b,
                               std::unique_ptr<Bucket> *bucket,
                               optional_yield y)override
        {
            return DaosStore::get_bucket(dpp, u, b, bucket, y);
        }
        virtual int get_bucket(User *u,
                               const RGWBucketInfo &i,
                               std::unique_ptr<Bucket> *bucket)override
        {
            return DaosStore::get_bucket(u, i, bucket);
        }
        virtual int get_bucket(const DoutPrefixProvider *dpp,
                               User *u,
                               const std::string &tenant,
                               const std::string &name,
                               std::unique_ptr<Bucket> *bucket,
                               optional_yield y)override
        {
            return DaosStore::get_bucket(dpp, u, tenant, name, bucket, y);
        }
        virtual bool is_meta_master(void)override
        {
            /** TBD - SRP : Confirm if using DAOS status for Master is sufficient. */
            return DaosStore::is_meta_master();
        }
        virtual int forward_request_to_master(const DoutPrefixProvider *dpp,
                                              User *user,
                                              obj_version *objv,
                                              bufferlist &in_data,
                                              JSONParser *jp, req_info &info,
                                              optional_yield y)override
        {
            return DaosStore::forward_request_to_master(dpp, user, objv,
                                                        in_data, jp, info, y);
        }
        virtual int forward_iam_request_to_master(const DoutPrefixProvider *dpp,
                                                  const RGWAccessKey &key,
                                                  obj_version *objv,
                                                  bufferlist &in_data,
                                                  RGWXMLDecoder::XMLParser *parser,
                                                  req_info &info,
                                                  optional_yield y)override
        {
            return DaosStore::forward_iam_request_to_master(dpp, key, objv,
                                                            in_data, parser,
                                                            info, y);
        }
        virtual Zone* get_zone(void)
        {
            return DaosStore::get_zone();
        }
        virtual std::string zone_unique_id(uint64_t unique_num)override
        {
            return DaosStore::zone_unique_id(unique_num);
        }
        virtual std::string zone_unique_trans_id(const uint64_t unique_num)override
        {
            return DaosStore::zone_unique_trans_id(unique_num);
        }
        virtual int cluster_stat(RGWClusterStat &stats)override
        {
            return DaosStore::cluster_stat(stats);
        }
        virtual std::unique_ptr<Lifecycle> get_lifecycle(void)override
        {
            return DaosStore::get_lifecycle();
        }
        virtual std::unique_ptr<Completions> get_completions(void)override
        {
            return DaosStore::get_completions();
        }
        virtual std::unique_ptr<Notification> get_notification(rgw::sal::Object *obj,
                                                               rgw::sal::Object *src_obj,
                                                               req_state *s,
                                                               rgw::notify::EventType event_type,
                                                               const std::string *object_name = nullptr)override
        {
            return DaosStore::get_notification(obj, src_obj, s, event_type, object_name);
        }
        virtual std::unique_ptr<Notification> get_notification(const DoutPrefixProvider *dpp, rgw::sal::Object *obj,
                                                               rgw::sal::Object *src_obj, rgw::notify::EventType event_type, rgw::sal::Bucket *_bucket,
                                                               std::string &_user_id, std::string &_user_tenant, std::string &_req_id, optional_yield y)override
        {
            return DaosStore::get_notification(dpp, obj, src_obj, event_type, _bucket, _user_id, _user_tenant, _req_id, y);
        }
        virtual RGWLC* get_rgwlc(void)override
        {
            return NULL;
        }
        virtual RGWCoroutinesManagerRegistry* get_cr_registry(void)override
        {
            return NULL;
        }

        virtual int log_usage(const DoutPrefixProvider *dpp,
                              std::map<rgw_user_bucket,
                              RGWUsageBatch> &usage_info)override
        {
            return DaosStore::log_usage(dpp, usage_info);
        }
        virtual int log_op(const DoutPrefixProvider *dpp,
                           std::string &oid,
                           bufferlist &bl)override
        {
            return DaosStore::log_op(dpp, oid, bl);
        }
        virtual int register_to_service_map(const DoutPrefixProvider *dpp,
                                            const std::string &daemon_type,
                                            const std::map<std::string, std::string> &meta)override
        {
            return DaosStore::register_to_service_map(dpp, daemon_type, meta);
        }
        virtual void get_quota(RGWQuota &quota)override
        {
            return DaosStore::get_quota(quota);
        }
        virtual void get_ratelimit(RGWRateLimitInfo &bucket_ratelimit,
                                   RGWRateLimitInfo &user_ratelimit,
                                   RGWRateLimitInfo &anon_ratelimit)override
        {
            return DaosStore::get_ratelimit(bucket_ratelimit, user_ratelimit,
                                            anon_ratelimit);
        }
        virtual int set_buckets_enabled(const DoutPrefixProvider *dpp,
                                        std::vector<rgw_bucket> &buckets,
                                        bool enabled)override
        {
            return DaosStore::set_buckets_enabled(dpp, buckets, enabled);
        }
        virtual int get_sync_policy_handler(const DoutPrefixProvider *dpp,
                                            std::optional<rgw_zone_id> zone,
                                            std::optional<rgw_bucket> bucket,
                                            RGWBucketSyncPolicyHandlerRef *phandler,
                                            optional_yield y)override
        {
            DaosStore::get_sync_policy_handler(dpp, zone, bucket, phandler, y);
        }
        virtual RGWDataSyncStatusManager* get_data_sync_manager(const rgw_zone_id &source_zone)override
        {
            return DaosStore::get_data_sync_manager(source_zone);
        }
        virtual void wakeup_meta_sync_shards(std::set<int> &shard_ids)override
        {
            return;
        }
        virtual void wakeup_data_sync_shards(const DoutPrefixProvider *dpp,
                                             const rgw_zone_id &source_zone,
                                             boost::container::flat_map<int,
                                             boost::container::flat_set<rgw_data_notify_entry>>& shard_ids) override
        {
            return;
        }
        virtual int clear_usage(const DoutPrefixProvider *dpp)override
        {
            return 0;
        }
        virtual int read_all_usage(const DoutPrefixProvider *dpp,
                                   uint64_t start_epoch, uint64_t end_epoch,
                                   uint32_t max_entries, bool *is_truncated,
                                   RGWUsageIter &usage_iter,
                                   std::map<rgw_user_bucket, rgw_usage_log_entry> &usage)override
        {
            return DaosStore::read_all_usage(dpp, start_epoch, end_epoch, max_entries, is_truncated, usage_iter, usage);
        }
        virtual int trim_all_usage(const DoutPrefixProvider *dpp,
                                   uint64_t start_epoch,
                                   uint64_t end_epoch)override
        {
            return DaosStore::trim_all_usage(dpp, start_epoch, end_epoch);
        }
        virtual int get_config_key_val(std::string name,
                                       bufferlist *bl)override
        {
            return DaosStore::get_config_key_val(name, bl);
        }
        virtual int meta_list_keys_init(const DoutPrefixProvider *dpp,
                                        const std::string &section,
                                        const std::string &marker,
                                        void **phandle)override
        {
            return DaosStore::meta_list_keys_init(dpp, section, marker, phandle);
        }
        virtual int meta_list_keys_next(const DoutPrefixProvider *dpp,
                                        void *handle,
                                        int max,
                                        std::list<std::string> &keys,
                                        bool *truncated)override
        {
            return DaosStore::meta_list_keys_next(dpp, handle, max, keys, truncated);
        }
        virtual void meta_list_keys_complete(void *handle)override
        {
            return;
        }
        virtual std::string meta_get_marker(void *handle)override
        {
            return DaosStore::meta_get_marker(handle);
        }
        virtual int meta_remove(const DoutPrefixProvider *dpp,
                                std::string &metadata_key,
                                optional_yield y)override
        {
            return DaosStore::meta_remove(dpp, metadata_key, y);
        }
        virtual const RGWSyncModuleInstanceRef &get_sync_module(void)
        {
            return sync_module;
        }
        virtual std::string get_host_id(void)
        {
            return "";
        }
        virtual std::unique_ptr<LuaManager> get_lua_manager(void)override
        {
            return std::make_unique<HybridLuaManager>(this);
        }
        virtual std::unique_ptr<RGWRole> get_role(std::string name,
                                                  std::string tenant,
                                                  std::string path = "",
                                                  std::string trust_policy = "",
                                                  std::string max_session_duration_str = "",
                                                  std::multimap<std::string, std::string> tags = {
        })override
        {
            return DaosStore::get_role(name, tenant, path, trust_policy, max_session_duration_str, tags);
        }
        virtual std::unique_ptr<RGWRole> get_role(const RGWRoleInfo &info)override
        {
            return DaosStore::get_role(info);
        }
        virtual std::unique_ptr<RGWRole> get_role(std::string id)override
        {
            return DaosStore::get_role(id);
        }
        virtual int get_roles(const DoutPrefixProvider *dpp,
                              optional_yield y,
                              const std::string &path_prefix,
                              const std::string &tenant,
                              std::vector<std::unique_ptr<RGWRole>>& roles) override
        {
            return DaosStore::get_roles(dpp, y, path_prefix, tenant, roles);
        }
        virtual std::unique_ptr<RGWOIDCProvider> get_oidc_provider(void)override
        {
            return DaosStore::get_oidc_provider();
        }
        virtual int get_oidc_providers(const DoutPrefixProvider *dpp,
                                       const std::string &tenant,
                                       std::vector<std::unique_ptr<RGWOIDCProvider>>& providers) override
        {
            return DaosStore::get_oidc_providers(dpp, tenant, providers);
        }
        virtual std::unique_ptr<Writer> get_append_writer(const DoutPrefixProvider *dpp,
                                                          optional_yield y,
                                                          std::unique_ptr<rgw::sal::Object> _head_obj,
                                                          const rgw_user &owner,
                                                          const rgw_placement_rule *ptail_placement_rule,
                                                          const std::string &unique_tag,
                                                          uint64_t position,
                                                          uint64_t *cur_accounted_size)override
        {
            return DaosStore::get_append_writer(dpp, y, _head_obj, owner, ptail_placement_rule, unique_tag, position, cur_accounted_size);
        }
        virtual std::unique_ptr<Writer> get_atomic_writer(const DoutPrefixProvider *dpp,
                                                          optional_yield y,
                                                          std::unique_ptr<rgw::sal::Object> _head_obj,
                                                          const rgw_user &owner,
                                                          const rgw_placement_rule *ptail_placement_rule,
                                                          uint64_t olh_epoch,
                                                          const std::string &unique_tag)override
        {
            return std::make_unique<HybridAtomicWriter>(dpp, y, std::move(_head_obj), this,
                                                      owner, ptail_placement_rule,
                                                      olh_epoch, unique_tag);
        }
        virtual const std::string &get_compression_type(const rgw_placement_rule &rule)override
        {
            return DaosStore::get_compression_type(rule);
        }
        virtual bool valid_placement(const rgw_placement_rule &rule)override
        {
            return DaosStore::valid_placement(rule);
        }
        virtual void finalize(void)override
        {
            DaosStore::finalize(void);
        }
        virtual CephContext* ctx(void)override
        {
            return DaosStore::ctx();
        }
        virtual const std::string &get_luarocks_path(void) const override
        {
            return DaosStore::get_luarocks_path();
        }
        virtual void set_luarocks_path(const std::string &path)override
        {
            return DaosStore::set_luarocks_path(path);
        }
    };
}  // namespace rgw::sal

extern "C" {
inline void* newHybridStore(CephContext *cct)
{
    rgw::sal::HybridStore *store = new rgw::sal::HybridStore(cct);

    //newDaosStore(cct, store);
    newMotrStore(cct,  (MotrStore *)store);
    return store;
}
}

