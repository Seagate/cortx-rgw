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
#include "rgw_sal_store.h"

namespace rgw::sal {

class HybridUser : public StoreUser {
  private:
    HybridStore         *store;

  public:
    HybridUser(HybridStore *_st, const rgw_user& _u) : StoreUser(_u), store(_st) { }
    HybridUser(HybridStore *_st, const RGWUserInfo& _i) : StoreUser(_i), store(_st) { }
    HybridUser(HybridStore *_st) : store(_st) { }
    HybridUser(HybridUser& _o) = default;
    HybridUser() {}

    virtual std::unique_ptr<User> clone() override {
      return std::unique_ptr<User>(new HybridUser(*this));
    }
    int list_buckets(const DoutPrefixProvider *dpp, const std::string& marker, const std::string& end_marker,
        uint64_t max, bool need_stats, BucketList& buckets, optional_yield y) override;
    virtual int create_bucket(const DoutPrefixProvider* dpp,
                            const rgw_bucket& b,
                            const std::string& zonegroup_id,
                            rgw_placement_rule& placement_rule,
                            std::string& swift_ver_location,
                            const RGWQuotaInfo* pquota_info,
                            const RGWAccessControlPolicy& policy,
                            Attrs& attrs,
                            RGWBucketInfo& info,
                            obj_version& ep_objv,
                            bool exclusive,
                            bool obj_lock_enabled,
                            bool* existed,
                            req_info& req_info,
                            std::unique_ptr<Bucket>* bucket,
                            optional_yield y) override;
    virtual int read_attrs(const DoutPrefixProvider* dpp, optional_yield y) override;
    virtual int merge_and_store_attrs(const DoutPrefixProvider* dpp, Attrs& new_attrs, optional_yield y) override;
    virtual int read_stats(const DoutPrefixProvider *dpp,
        optional_yield y, RGWStorageStats* stats,
        ceph::real_time *last_stats_sync = nullptr,
        ceph::real_time *last_stats_update = nullptr) override;
    virtual int read_stats_async(const DoutPrefixProvider *dpp, RGWGetUserStats_CB* cb) override;
    virtual int complete_flush_stats(const DoutPrefixProvider *dpp, optional_yield y) override;
    virtual int read_usage(const DoutPrefixProvider *dpp, uint64_t start_epoch, uint64_t end_epoch, uint32_t max_entries,
        bool* is_truncated, RGWUsageIter& usage_iter,
        std::map<rgw_user_bucket, rgw_usage_log_entry>& usage) override;
    virtual int trim_usage(const DoutPrefixProvider *dpp, uint64_t start_epoch, uint64_t end_epoch) override;

    virtual int load_user(const DoutPrefixProvider* dpp, optional_yield y) override;
    virtual int store_user(const DoutPrefixProvider* dpp, optional_yield y, bool exclusive, RGWUserInfo* old_info = nullptr) override;
    virtual int remove_user(const DoutPrefixProvider* dpp, optional_yield y) override;

    int create_user_info_idx();

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

    void encode(bufferlist& bl)  const
    {
      ENCODE_START(4, 4, bl);
      encode(info, bl);
      encode(bucket_version, bl);
      encode(mtime, bl);
      encode(bucket_attrs, bl); //rgw_cache.h example for a map
      ENCODE_FINISH(bl);
    }

    void decode(bufferlist::const_iterator& bl)
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
      acls() {
      }

    HybridBucket(HybridStore *_st, User* _u)
      : StoreBucket(_u),
      store(_st),
      acls() {
      }

    HybridBucket(HybridStore *_st, const rgw_bucket& _b)
      : StoreBucket(_b),
      store(_st),
      acls() {
      }

    HybridBucket(HybridStore *_st, const RGWBucketEnt& _e)
      : StoreBucket(_e),
      store(_st),
      acls() {
      }

    HybridBucket(HybridStore *_st, const RGWBucketInfo& _i)
      : StoreBucket(_i),
      store(_st),
      acls() {
      }

    HybridBucket(HybridStore *_st, const rgw_bucket& _b, User* _u)
      : StoreBucket(_b, _u),
      store(_st),
      acls() {
      }

    HybridBucket(HybridStore *_st, const RGWBucketEnt& _e, User* _u)
      : StoreBucket(_e, _u),
      store(_st),
      acls() {
      }

    HybridBucket(HybridStore *_st, const RGWBucketInfo& _i, User* _u)
      : StoreBucket(_i, _u),
      store(_st),
      acls() {
      }

    ~HybridBucket() { }

    virtual std::unique_ptr<Object> get_object(const rgw_obj_key& k) override;
    virtual int list(const DoutPrefixProvider *dpp, ListParams&, int, ListResults&, optional_yield y) override;
    virtual int remove_bucket(const DoutPrefixProvider *dpp, bool delete_children, bool forward_to_master, req_info* req_info, optional_yield y) override;
    virtual int remove_bucket_bypass_gc(int concurrent_max, bool
        keep_index_consistent,
        optional_yield y, const
        DoutPrefixProvider *dpp) override;
    virtual RGWAccessControlPolicy& get_acl(void) override { return acls; }
    virtual int set_acl(const DoutPrefixProvider *dpp, RGWAccessControlPolicy& acl, optional_yield y) override;
    virtual int load_bucket(const DoutPrefixProvider *dpp, optional_yield y, bool get_stats = false) override;
    int link_user(const DoutPrefixProvider* dpp, User* new_user, optional_yield y);
    int unlink_user(const DoutPrefixProvider* dpp, User* new_user, optional_yield y);
    int create_bucket_index();
    int create_multipart_indices();
    virtual int read_stats(const DoutPrefixProvider *dpp,
        const bucket_index_layout_generation& idx_layout, int shard_id,
        std::string *bucket_ver, std::string *master_ver,
        std::map<RGWObjCategory, RGWStorageStats>& stats,
        std::string *max_marker = nullptr,
        bool *syncstopped = nullptr) override;
    virtual int read_stats_async(const DoutPrefixProvider *dpp,
                                 const bucket_index_layout_generation& idx_layout,
                                 int shard_id, RGWGetBucketStats_CB* ctx) override;
    virtual int sync_user_stats(const DoutPrefixProvider *dpp, optional_yield y) override;
    virtual int update_container_stats(const DoutPrefixProvider *dpp) override;
    virtual int check_bucket_shards(const DoutPrefixProvider *dpp) override;
    virtual int chown(const DoutPrefixProvider *dpp, User* new_user, User* old_user, optional_yield y, const std::string* marker = nullptr) override;
    virtual int put_info(const DoutPrefixProvider *dpp, bool exclusive, ceph::real_time mtime) override;
    virtual bool is_owner(User* user) override;
    virtual int check_empty(const DoutPrefixProvider *dpp, optional_yield y) override;
    virtual int check_quota(const DoutPrefixProvider *dpp, RGWQuota& quota, uint64_t obj_size, optional_yield y, bool check_size_only = false) override;
    virtual int merge_and_store_attrs(const DoutPrefixProvider *dpp, Attrs& attrs, optional_yield y) override;
    virtual int try_refresh_info(const DoutPrefixProvider *dpp, ceph::real_time *pmtime) override;
    virtual int read_usage(const DoutPrefixProvider *dpp, uint64_t start_epoch, uint64_t end_epoch, uint32_t max_entries,
        bool *is_truncated, RGWUsageIter& usage_iter,
        std::map<rgw_user_bucket, rgw_usage_log_entry>& usage) override;
    virtual int trim_usage(const DoutPrefixProvider *dpp, uint64_t start_epoch, uint64_t end_epoch) override;
    virtual int remove_objs_from_index(const DoutPrefixProvider *dpp, std::list<rgw_obj_index_key>& objs_to_unlink) override;
    virtual int check_index(const DoutPrefixProvider *dpp, std::map<RGWObjCategory, RGWStorageStats>& existing_stats, std::map<RGWObjCategory, RGWStorageStats>& calculated_stats) override;
    virtual int rebuild_index(const DoutPrefixProvider *dpp) override;
    virtual int set_tag_timeout(const DoutPrefixProvider *dpp, uint64_t timeout) override;
    virtual int purge_instance(const DoutPrefixProvider *dpp) override;
    virtual std::unique_ptr<Bucket> clone() override {
      return std::make_unique<HybridBucket>(*this);
    }
    virtual std::unique_ptr<MultipartUpload> get_multipart_upload(const std::string& oid,
                                std::optional<std::string> upload_id=std::nullopt,
                                ACLOwner owner={}, ceph::real_time mtime=real_clock::now()) override;
    virtual int list_multiparts(const DoutPrefixProvider *dpp,
      const std::string& prefix,
      std::string& marker,
      const std::string& delim,
      const int& max_uploads,
      std::vector<std::unique_ptr<MultipartUpload>>& uploads,
      std::map<std::string, bool> *common_prefixes,
      bool *is_truncated) override;
    virtual int abort_multiparts(const DoutPrefixProvider *dpp, CephContext *cct) override;

    friend class HybridStore;
};

class HybridPlacementTier: public StorePlacementTier {
  HybridStore* store;
  RGWZoneGroupPlacementTier tier;
public:
  HybridPlacementTier(HybridStore* _store, const RGWZoneGroupPlacementTier& _tier) : store(_store), tier(_tier) {}
  virtual ~HybridPlacementTier() = default;

  virtual const std::string& get_tier_type() { return tier.tier_type; }
  virtual const std::string& get_storage_class() { return tier.storage_class; }
  virtual bool retain_head_object() { return tier.retain_head_object; }
  RGWZoneGroupPlacementTier& get_rt() { return tier; }
};

class HybridZoneGroup : public StoreZoneGroup {
  HybridStore* store;
  const RGWZoneGroup group;
  std::string empty;
public:
  HybridZoneGroup(HybridStore* _store) : store(_store), group() {}
  HybridZoneGroup(HybridStore* _store, const RGWZoneGroup& _group) : store(_store), group(_group) {}
  virtual ~HybridZoneGroup() = default;

  virtual const std::string& get_id() const override { return group.get_id(); };
  virtual const std::string& get_name() const override { return group.get_name(); };
  virtual int equals(const std::string& other_zonegroup) const override {
    return group.equals(other_zonegroup);
  };
  /** Get the endpoint from zonegroup, or from master zone if not set */
  virtual const std::string& get_endpoint() const override;
  virtual bool placement_target_exists(std::string& target) const override;
  virtual bool is_master_zonegroup() const override {
    return group.is_master_zonegroup();
  };
  virtual const std::string& get_api_name() const override { return group.api_name; };
  virtual int get_placement_target_names(std::set<std::string>& names) const override;
  virtual const std::string& get_default_placement_name() const override {
    return group.default_placement.name; };
  virtual int get_hostnames(std::list<std::string>& names) const override {
    names = group.hostnames;
    return 0;
  };
  virtual int get_s3website_hostnames(std::list<std::string>& names) const override {
   names = group.hostnames_s3website;
    return 0;
  };
  virtual int get_zone_count() const override {
    return group.zones.size();
  }
  virtual int get_placement_tier(const rgw_placement_rule& rule, std::unique_ptr<PlacementTier>* tier);
  const RGWZoneGroup& get_group() { return group; }
  virtual std::unique_ptr<ZoneGroup> clone() override {
    return std::make_unique<HybridZoneGroup>(store, group);
  }
};

class HybridZone : public StoreZone {
  protected:
    HybridStore* store;
    RGWRealm *realm{nullptr};
    HybridZoneGroup zonegroup;
    RGWZone *zone_public_config{nullptr}; /* external zone params, e.g., entrypoints, log flags, etc. */
    RGWZoneParams *zone_params{nullptr}; /* internal zone params, e.g., rados pools */
    RGWPeriod *current_period{nullptr};
    rgw_zone_id cur_zone_id;

  public:
    HybridZone(HybridStore* _store) : store(_store), zonegroup(_store) {
      realm = new RGWRealm();
      zone_public_config = new RGWZone();
      zone_params = new RGWZoneParams();
      current_period = new RGWPeriod();
      cur_zone_id = rgw_zone_id(zone_params->get_id());

      // XXX: only default and STANDARD supported for now
      RGWZonePlacementInfo info;
      RGWZoneStorageClasses sc;
      sc.set_storage_class("STANDARD", nullptr, nullptr);
      info.storage_classes = sc;
      zone_params->placement_pools["default"] = info;
    }
    HybridZone(HybridStore* _store, HybridZoneGroup _zg) : store(_store), zonegroup(_zg) {
      realm = new RGWRealm();
      zone_public_config = new RGWZone();
      zone_params = new RGWZoneParams();
      current_period = new RGWPeriod();
      cur_zone_id = rgw_zone_id(zone_params->get_id());

      // XXX: only default and STANDARD supported for now
      RGWZonePlacementInfo info;
      RGWZoneStorageClasses sc;
      sc.set_storage_class("STANDARD", nullptr, nullptr);
      info.storage_classes = sc;
      zone_params->placement_pools["default"] = info;
    }
    ~HybridZone() = default;

    virtual std::unique_ptr<Zone> clone() override {
      return std::make_unique<HybridZone>(store);
    }
    virtual ZoneGroup& get_zonegroup() override;
    virtual int get_zonegroup(const std::string& id, std::unique_ptr<ZoneGroup>* zonegroup) override;
    virtual const rgw_zone_id& get_id() override;
    virtual const std::string& get_name() const override;
    virtual bool is_writeable() override;
    virtual bool get_redirect_endpoint(std::string* endpoint) override;
    virtual bool has_zonegroup_api(const std::string& api) const override;
    virtual const std::string& get_current_period_id() override;
    virtual const RGWAccessKey& get_system_key() { return zone_params->system_key; }
    virtual const std::string& get_realm_name() { return realm->get_name(); }
    virtual const std::string& get_realm_id() { return realm->get_id(); }
    virtual const std::string_view get_tier_type() { return "rgw"; }
    friend class HybridStore;
};

class HybridLuaManager : public StoreLuaManager {
  HybridStore* store;

  public:
  HybridLuaManager(HybridStore* _s) : store(_s)
  {
  }
  virtual ~HybridLuaManager() = default;

  /** Get a script named with the given key from the backing store */
  virtual int get_script(const DoutPrefixProvider* dpp, optional_yield y, const std::string& key, std::string& script) override;
  /** Put a script named with the given key to the backing store */
  virtual int put_script(const DoutPrefixProvider* dpp, optional_yield y, const std::string& key, const std::string& script) override;
  /** Delete a script named with the given key from the backing store */
  virtual int del_script(const DoutPrefixProvider* dpp, optional_yield y, const std::string& key) override;
  /** Add a lua package */
  virtual int add_package(const DoutPrefixProvider* dpp, optional_yield y, const std::string& package_name) override;
  /** Remove a lua package */
  virtual int remove_package(const DoutPrefixProvider* dpp, optional_yield y, const std::string& package_name) override;
  /** List lua packages */
  virtual int list_packages(const DoutPrefixProvider* dpp, optional_yield y, rgw::lua::packages_t& packages) override;
};

class HybridOIDCProvider : public RGWOIDCProvider {
  HybridStore* store;
  public:
  HybridOIDCProvider(HybridStore* _store) : store(_store) {}
  ~HybridOIDCProvider() = default;

  virtual int store_url(const DoutPrefixProvider *dpp, const std::string& url, bool exclusive, optional_yield y) override { return 0; }
  virtual int read_url(const DoutPrefixProvider *dpp, const std::string& url, const std::string& tenant) override { return 0; }
  virtual int delete_obj(const DoutPrefixProvider *dpp, optional_yield y) override { return 0;}

  void encode(bufferlist& bl) const {
    RGWOIDCProvider::encode(bl);
  }
  void decode(bufferlist::const_iterator& bl) {
    RGWOIDCProvider::decode(bl);
  }
};

class HybridObject : public StoreObject {
  private:
    HybridStore *store;
    RGWAccessControlPolicy acls;
    RGWObjCategory category;

    // If this object is pat of a multipart uploaded one.
    // TODO: do it in another class? HybridPartObject : public HybridObject
    uint64_t part_off;
    uint64_t part_size;
    uint64_t part_num;

  public:

    // motr object metadata stored in index
    struct Meta {
      struct m0_uint128 oid = {};
      struct m0_fid pver = {};
      uint64_t layout_id = 0;

      void encode(bufferlist& bl) const
      {
        ENCODE_START(5, 5, bl);
        encode(oid.u_hi, bl);
        encode(oid.u_lo, bl);
        encode(pver.f_container, bl);
        encode(pver.f_key, bl);
        encode(layout_id, bl);
        ENCODE_FINISH(bl);
      }

      void decode(bufferlist::const_iterator& bl)
      {
        DECODE_START(5, bl);
        decode(oid.u_hi, bl);
        decode(oid.u_lo, bl);
        decode(pver.f_container, bl);
        decode(pver.f_key, bl);
        decode(layout_id, bl);
        DECODE_FINISH(bl);
      }
    };

    struct m0_obj     *mobj = NULL;
    Meta               meta;

    struct HybridReadOp : public ReadOp {
      private:
        HybridObject* source;

	// The set of part objects if the source is
	// a multipart uploaded object.
        std::map<int, std::unique_ptr<HybridObject>> part_objs;

      public:
        HybridReadOp(HybridObject *_source);

        virtual int prepare(optional_yield y, const DoutPrefixProvider* dpp) override;
        virtual int read(int64_t off, int64_t end, bufferlist& bl, optional_yield y, const DoutPrefixProvider* dpp) override;
        virtual int iterate(const DoutPrefixProvider* dpp, int64_t off, int64_t end, RGWGetDataCB* cb, optional_yield y) override;
        virtual int get_attr(const DoutPrefixProvider* dpp, const char* name, bufferlist& dest, optional_yield y) override;
    };

    struct HybridDeleteOp : public DeleteOp {
      private:
        HybridObject* source;

      public:
        HybridDeleteOp(HybridObject* _source);

        virtual int delete_obj(const DoutPrefixProvider* dpp, optional_yield y) override;
    };

    HybridObject() = default;

    HybridObject(HybridStore *_st, const rgw_obj_key& _k)
      : StoreObject(_k), store(_st), acls() {}
    HybridObject(HybridStore *_st, const rgw_obj_key& _k, Bucket* _b)
      : StoreObject(_k, _b), store(_st), acls() {}

    HybridObject(HybridObject& _o) = default;

    virtual ~HybridObject();

    virtual int delete_object(const DoutPrefixProvider* dpp,
        optional_yield y,
        bool prevent_versioning = false) override;
    virtual int delete_obj_aio(const DoutPrefixProvider* dpp, RGWObjState* astate, Completions* aio,
        bool keep_index_consistent, optional_yield y) override;
    virtual int copy_object(User* user,
        req_info* info, const rgw_zone_id& source_zone,
        rgw::sal::Object* dest_object, rgw::sal::Bucket* dest_bucket,
        rgw::sal::Bucket* src_bucket,
        const rgw_placement_rule& dest_placement,
        ceph::real_time* src_mtime, ceph::real_time* mtime,
        const ceph::real_time* mod_ptr, const ceph::real_time* unmod_ptr,
        bool high_precision_time,
        const char* if_match, const char* if_nomatch,
        AttrsMod attrs_mod, bool copy_if_newer, Attrs& attrs,
        RGWObjCategory category, uint64_t olh_epoch,
        boost::optional<ceph::real_time> delete_at,
        std::string* version_id, std::string* tag, std::string* etag,
        void (*progress_cb)(off_t, void *), void* progress_data,
        const DoutPrefixProvider* dpp, optional_yield y) override;
    virtual RGWAccessControlPolicy& get_acl(void) override { return acls; }
    virtual int set_acl(const RGWAccessControlPolicy& acl) override { acls = acl; return 0; }
    virtual int get_obj_state(const DoutPrefixProvider* dpp, RGWObjState **state, optional_yield y, bool follow_olh = true) override;
    virtual int set_obj_attrs(const DoutPrefixProvider* dpp, Attrs* setattrs, Attrs* delattrs, optional_yield y) override;
    virtual int get_obj_attrs(optional_yield y, const DoutPrefixProvider* dpp, rgw_obj* target_obj = NULL) override;
    virtual int modify_obj_attrs(const char* attr_name, bufferlist& attr_val, optional_yield y, const DoutPrefixProvider* dpp) override;
    virtual int delete_obj_attrs(const DoutPrefixProvider* dpp, const char* attr_name, optional_yield y) override;
    virtual bool is_expired() override;
    virtual void gen_rand_obj_instance_name() override;
    virtual std::unique_ptr<Object> clone() override {
      return std::unique_ptr<Object>(new HybridObject(*this));
    }
    virtual std::unique_ptr<MPSerializer> get_serializer(const DoutPrefixProvider *dpp, const std::string& lock_name) override;
    virtual int transition(Bucket* bucket,
        const rgw_placement_rule& placement_rule,
        const real_time& mtime,
        uint64_t olh_epoch,
        const DoutPrefixProvider* dpp,
        optional_yield y) override;
    virtual int transition_to_cloud(Bucket* bucket,
			   rgw::sal::PlacementTier* tier,
			   rgw_bucket_dir_entry& o,
			   std::set<std::string>& cloud_targets,
			   CephContext* cct,
			   bool update_object,
			   const DoutPrefixProvider* dpp,
			   optional_yield y) override;
    virtual bool placement_rules_match(rgw_placement_rule& r1, rgw_placement_rule& r2) override;
    virtual int dump_obj_layout(const DoutPrefixProvider *dpp, optional_yield y, Formatter* f) override;

    /* Swift versioning */
    virtual int swift_versioning_restore(bool& restored,
        const DoutPrefixProvider* dpp) override;
    virtual int swift_versioning_copy(const DoutPrefixProvider* dpp,
        optional_yield y) override;

    /* OPs */
    virtual std::unique_ptr<ReadOp> get_read_op() override;
    virtual std::unique_ptr<DeleteOp> get_delete_op() override;

    /* OMAP */
    virtual int omap_get_vals(const DoutPrefixProvider *dpp, const std::string& marker, uint64_t count,
        std::map<std::string, bufferlist> *m,
        bool* pmore, optional_yield y) override;
    virtual int omap_get_all(const DoutPrefixProvider *dpp, std::map<std::string, bufferlist> *m,
        optional_yield y) override;
    virtual int omap_get_vals_by_keys(const DoutPrefixProvider *dpp, const std::string& oid,
        const std::set<std::string>& keys,
        Attrs* vals) override;
    virtual int omap_set_val_by_key(const DoutPrefixProvider *dpp, const std::string& key, bufferlist& val,
        bool must_exist, optional_yield y) override;
  private:
    //int read_attrs(const DoutPrefixProvider* dpp, Hybrid::Object::Read &read_op, optional_yield y, rgw_obj* target_obj = nullptr);

  public:
    bool is_opened() { return mobj != NULL; }
    int create_mobj(const DoutPrefixProvider *dpp, uint64_t sz);
    int open_mobj(const DoutPrefixProvider *dpp);
    int delete_mobj(const DoutPrefixProvider *dpp);
    void close_mobj();
    int write_mobj(const DoutPrefixProvider *dpp, bufferlist&& data, uint64_t offset);
    int read_mobj(const DoutPrefixProvider* dpp, int64_t off, int64_t end, RGWGetDataCB* cb);
    unsigned get_optimal_bs(unsigned len);

    int get_part_objs(const DoutPrefixProvider *dpp,
                      std::map<int, std::unique_ptr<HybridObject>>& part_objs);
    int open_part_objs(const DoutPrefixProvider* dpp,
                       std::map<int, std::unique_ptr<HybridObject>>& part_objs);
    int read_multipart_obj(const DoutPrefixProvider* dpp,
                           int64_t off, int64_t end, RGWGetDataCB* cb,
                           std::map<int, std::unique_ptr<HybridObject>>& part_objs);
    int delete_part_objs(const DoutPrefixProvider* dpp);
    void set_category(RGWObjCategory _category) {category = _category;}
    int get_bucket_dir_ent(const DoutPrefixProvider *dpp, rgw_bucket_dir_entry& ent);
    int update_version_entries(const DoutPrefixProvider *dpp);
};

// A placeholder locking class for multipart upload.
// TODO: implement it using Hybrid object locks.
class MPHybridSerializer : public StoreMPSerializer {

  public:
    MPHybridSerializer(const DoutPrefixProvider *dpp, HybridStore* store, HybridObject* obj, const std::string& lock_name) {}

    virtual int try_lock(const DoutPrefixProvider *dpp, utime_t dur, optional_yield y) override {return 0; }
    virtual int unlock() override { return 0;}
};

class HybridAtomicWriter : public StoreWriter {
  protected:
  rgw::sal::HybridStore* store;
  const rgw_user& owner;
  const rgw_placement_rule *ptail_placement_rule;
  uint64_t olh_epoch;
  const std::string& unique_tag;
  HybridObject obj;
  uint64_t total_data_size; // for total data being uploaded
  bufferlist acc_data;  // accumulated data
  uint64_t   acc_off; // accumulated data offset

  struct m0_bufvec buf;
  struct m0_bufvec attr;
  struct m0_indexvec ext;

  public:
  HybridAtomicWriter(const DoutPrefixProvider *dpp,
          optional_yield y,
          std::unique_ptr<rgw::sal::Object> _head_obj,
          HybridStore* _store,
          const rgw_user& _owner,
          const rgw_placement_rule *_ptail_placement_rule,
          uint64_t _olh_epoch,
          const std::string& _unique_tag);
  ~HybridAtomicWriter() = default;

  // prepare to start processing object data
  virtual int prepare(optional_yield y) override;

  // Process a bufferlist
  virtual int process(bufferlist&& data, uint64_t offset) override;

  int write();

  // complete the operation and make its result visible to clients
  virtual int complete(size_t accounted_size, const std::string& etag,
                       ceph::real_time *mtime, ceph::real_time set_mtime,
                       std::map<std::string, bufferlist>& attrs,
                       ceph::real_time delete_at,
                       const char *if_match, const char *if_nomatch,
                       const std::string *user_data,
                       rgw_zone_set *zones_trace, bool *canceled,
                       optional_yield y) override;

  unsigned populate_bvec(unsigned len, bufferlist::iterator &bi);
  void cleanup();
};

class HybridMultipartWriter : public StoreWriter {
protected:
  rgw::sal::HybridStore* store;

  // Head object.
  std::unique_ptr<rgw::sal::Object> head_obj;

  // Part parameters.
  const uint64_t part_num;
  const std::string part_num_str;
  std::unique_ptr<HybridObject> part_obj;
  uint64_t actual_part_size = 0;

public:
  HybridMultipartWriter(const DoutPrefixProvider *dpp,
		       optional_yield y, MultipartUpload* upload,
		       std::unique_ptr<rgw::sal::Object> _head_obj,
		       HybridStore* _store,
		       const rgw_user& owner,
		       const rgw_placement_rule *ptail_placement_rule,
		       uint64_t _part_num, const std::string& part_num_str) :
				  StoreWriter(dpp, y), store(_store), head_obj(std::move(_head_obj)),
				  part_num(_part_num), part_num_str(part_num_str)
  {
  }
  ~HybridMultipartWriter() = default;

  // prepare to start processing object data
  virtual int prepare(optional_yield y) override;

  // Process a bufferlist
  virtual int process(bufferlist&& data, uint64_t offset) override;

  // complete the operation and make its result visible to clients
  virtual int complete(size_t accounted_size, const std::string& etag,
                       ceph::real_time *mtime, ceph::real_time set_mtime,
                       std::map<std::string, bufferlist>& attrs,
                       ceph::real_time delete_at,
                       const char *if_match, const char *if_nomatch,
                       const std::string *user_data,
                       rgw_zone_set *zones_trace, bool *canceled,
                       optional_yield y) override;
};

class HybridMultipartPart : public StoreMultipartPart {
protected:
  RGWUploadPartInfo info;

public:
  HybridObject::Meta  meta;

  HybridMultipartPart(RGWUploadPartInfo _info, HybridObject::Meta _meta) :
    info(_info), meta(_meta) {}
  virtual ~HybridMultipartPart() = default;

  virtual uint32_t get_num() { return info.num; }
  virtual uint64_t get_size() { return info.accounted_size; }
  virtual const std::string& get_etag() { return info.etag; }
  virtual ceph::real_time& get_mtime() { return info.modified; }

  RGWObjManifest& get_manifest() { return info.manifest; }

  friend class HybridMultipartUpload;
};

class HybridMultipartUpload : public StoreMultipartUpload {
  HybridStore* store;
  RGWMPObj mp_obj;
  ACLOwner owner;
  ceph::real_time mtime;
  rgw_placement_rule placement;
  RGWObjManifest manifest;

public:
  HybridMultipartUpload(HybridStore* _store, Bucket* _bucket, const std::string& oid,
                      std::optional<std::string> upload_id, ACLOwner _owner, ceph::real_time _mtime) :
       StoreMultipartUpload(_bucket), store(_store), mp_obj(oid, upload_id), owner(_owner), mtime(_mtime) {}
  virtual ~HybridMultipartUpload() = default;

  virtual const std::string& get_meta() const { return mp_obj.get_meta(); }
  virtual const std::string& get_key() const { return mp_obj.get_key(); }
  virtual const std::string& get_upload_id() const { return mp_obj.get_upload_id(); }
  virtual const ACLOwner& get_owner() const override { return owner; }
  virtual ceph::real_time& get_mtime() { return mtime; }
  virtual std::unique_ptr<rgw::sal::Object> get_meta_obj() override;
  virtual int init(const DoutPrefixProvider* dpp, optional_yield y, ACLOwner& owner, rgw_placement_rule& dest_placement, rgw::sal::Attrs& attrs) override;
  virtual int list_parts(const DoutPrefixProvider* dpp, CephContext* cct,
			 int num_parts, int marker,
			 int* next_marker, bool* truncated,
			 bool assume_unsorted = false) override;
  virtual int abort(const DoutPrefixProvider* dpp, CephContext* cct) override;
  virtual int complete(const DoutPrefixProvider* dpp,
		       optional_yield y, CephContext* cct,
		       std::map<int, std::string>& part_etags,
		       std::list<rgw_obj_index_key>& remove_objs,
		       uint64_t& accounted_size, bool& compressed,
		       RGWCompressionInfo& cs_info, off_t& off,
		       std::string& tag, ACLOwner& owner,
		       uint64_t olh_epoch,
		       rgw::sal::Object* target_obj) override;
  virtual int get_info(const DoutPrefixProvider *dpp, optional_yield y, rgw_placement_rule** rule, rgw::sal::Attrs* attrs = nullptr) override;
  virtual std::unique_ptr<Writer> get_writer(const DoutPrefixProvider *dpp,
			  optional_yield y,
			  std::unique_ptr<rgw::sal::Object> _head_obj,
			  const rgw_user& owner,
			  const rgw_placement_rule *ptail_placement_rule,
			  uint64_t part_num,
			  const std::string& part_num_str) override;
  int delete_parts(const DoutPrefixProvider *dpp);
};

class HybridStore : public StoreStore {
 private:
 std::vector<Store *> stores;
 std::string store_name;
 RGWStorePOlic *store_policy;

 public:
  CephContext* cctx;

  HybridStore(CephContext* c, const StoreManager::Config& cfg) {
    cctx(c);
    store_name = cfg.store_name
  }
  ~HybridStore() = default;

  virtual const std::string get_name() const override { return store_name; }

  virtual std::unique_ptr<User> get_user(const rgw_user& u) override;
  virtual std::string get_cluster_id(const DoutPrefixProvider* dpp,
                                     optional_yield y) override;
  virtual int get_user_by_access_key(const DoutPrefixProvider* dpp,
                                     const std::string& key, optional_yield y,
                                     std::unique_ptr<User>* user) override;
  virtual int get_user_by_email(const DoutPrefixProvider* dpp,
                                const std::string& email, optional_yield y,
                                std::unique_ptr<User>* user) override;
  virtual int get_user_by_swift(const DoutPrefixProvider* dpp,
                                const std::string& user_str, optional_yield y,
                                std::unique_ptr<User>* user) override;
  virtual std::unique_ptr<Object> get_object(const rgw_obj_key& k) override;
  virtual int get_bucket(const DoutPrefixProvider* dpp, User* u,
                         const rgw_bucket& b, std::unique_ptr<Bucket>* bucket,
                         optional_yield y) override;
  virtual int get_bucket(User* u, const RGWBucketInfo& i,
                         std::unique_ptr<Bucket>* bucket) override;
  virtual int get_bucket(const DoutPrefixProvider* dpp, User* u,
                         const std::string& tenant, const std::string& name,
                         std::unique_ptr<Bucket>* bucket,
                         optional_yield y) override;
  virtual bool is_meta_master() override;
  virtual int forward_request_to_master(const DoutPrefixProvider* dpp,
                                        User* user, obj_version* objv,
                                        bufferlist& in_data, JSONParser* jp,
                                        req_info& info,
                                        optional_yield y) override;
  virtual int forward_iam_request_to_master(
      const DoutPrefixProvider* dpp, const RGWAccessKey& key, obj_version* objv,
      bufferlist& in_data, RGWXMLDecoder::XMLParser* parser, req_info& info,
      optional_yield y) override;
  virtual Zone* get_zone() { return &zone; }
  virtual std::string zone_unique_id(uint64_t unique_num) override;
  virtual std::string zone_unique_trans_id(const uint64_t unique_num) override;
  virtual int cluster_stat(RGWClusterStat& stats) override;
  virtual std::unique_ptr<Lifecycle> get_lifecycle(void) override;
  virtual std::unique_ptr<Completions> get_completions(void) override;
  virtual std::unique_ptr<Notification> get_notification(
      rgw::sal::Object* obj, rgw::sal::Object* src_obj, struct req_state* s,
      rgw::notify::EventType event_type,
      const std::string* object_name = nullptr) override;
  virtual std::unique_ptr<Notification> get_notification(
      const DoutPrefixProvider* dpp, rgw::sal::Object* obj,
      rgw::sal::Object* src_obj, rgw::notify::EventType event_type,
      rgw::sal::Bucket* _bucket, std::string& _user_id,
      std::string& _user_tenant, std::string& _req_id,
      optional_yield y) override;
  virtual RGWLC* get_rgwlc(void) override { return NULL; }
  virtual RGWCoroutinesManagerRegistry* get_cr_registry() override {
    return NULL;
  }

  virtual int log_usage(
      const DoutPrefixProvider* dpp,
      std::map<rgw_user_bucket, RGWUsageBatch>& usage_info) override;
  virtual int log_op(const DoutPrefixProvider* dpp, std::string& oid,
                     bufferlist& bl) override;
  virtual int register_to_service_map(
      const DoutPrefixProvider* dpp, const std::string& daemon_type,
      const std::map<std::string, std::string>& meta) override;
  virtual void get_quota(RGWQuota& quota) override;
  virtual void get_ratelimit(RGWRateLimitInfo& bucket_ratelimit,
                             RGWRateLimitInfo& user_ratelimit,
                             RGWRateLimitInfo& anon_ratelimit) override;
  virtual int set_buckets_enabled(const DoutPrefixProvider* dpp,
                                  std::vector<rgw_bucket>& buckets,
                                  bool enabled) override;
  virtual uint64_t get_new_req_id() override {
    return DAOS_NOT_IMPLEMENTED_LOG(nullptr);
  }
  virtual int get_sync_policy_handler(const DoutPrefixProvider* dpp,
                                      std::optional<rgw_zone_id> zone,
                                      std::optional<rgw_bucket> bucket,
                                      RGWBucketSyncPolicyHandlerRef* phandler,
                                      optional_yield y) override;
  virtual RGWDataSyncStatusManager* get_data_sync_manager(
      const rgw_zone_id& source_zone) override;
  virtual void wakeup_meta_sync_shards(std::set<int>& shard_ids) override {
    return;
  }
  virtual void wakeup_data_sync_shards(
      const DoutPrefixProvider* dpp, const rgw_zone_id& source_zone,
      boost::container::flat_map<
          int, boost::container::flat_set<rgw_data_notify_entry>>& shard_ids)
      override {
    return;
  }
  virtual int clear_usage(const DoutPrefixProvider* dpp) override {
    return DAOS_NOT_IMPLEMENTED_LOG(dpp);
  }
  virtual int read_all_usage(
      const DoutPrefixProvider* dpp, uint64_t start_epoch, uint64_t end_epoch,
      uint32_t max_entries, bool* is_truncated, RGWUsageIter& usage_iter,
      std::map<rgw_user_bucket, rgw_usage_log_entry>& usage) override;
  virtual int trim_all_usage(const DoutPrefixProvider* dpp,
                             uint64_t start_epoch, uint64_t end_epoch) override;
  virtual int get_config_key_val(std::string name, bufferlist* bl) override;
  virtual int meta_list_keys_init(const DoutPrefixProvider* dpp,
                                  const std::string& section,
                                  const std::string& marker,
                                  void** phandle) override;
  virtual int meta_list_keys_next(const DoutPrefixProvider* dpp, void* handle,
                                  int max, std::list<std::string>& keys,
                                  bool* truncated) override;
  virtual void meta_list_keys_complete(void* handle) override;
  virtual std::string meta_get_marker(void* handle) override;
  virtual int meta_remove(const DoutPrefixProvider* dpp,
                          std::string& metadata_key, optional_yield y) override;

  virtual const RGWSyncModuleInstanceRef& get_sync_module() {
    return sync_module;
  }
  virtual std::string get_host_id() { return ""; }

  virtual std::unique_ptr<LuaManager> get_lua_manager() override;
  virtual std::unique_ptr<RGWRole> get_role(
      std::string name, std::string tenant, std::string path = "",
      std::string trust_policy = "", std::string max_session_duration_str = "",
      std::multimap<std::string, std::string> tags = {}) override;
  virtual std::unique_ptr<RGWRole> get_role(const RGWRoleInfo& info) override;
  virtual std::unique_ptr<RGWRole> get_role(std::string id) override;
  virtual int get_roles(const DoutPrefixProvider* dpp, optional_yield y,
                        const std::string& path_prefix,
                        const std::string& tenant,
                        std::vector<std::unique_ptr<RGWRole>>& roles) override;
  virtual std::unique_ptr<RGWOIDCProvider> get_oidc_provider() override;
  virtual int get_oidc_providers(
      const DoutPrefixProvider* dpp, const std::string& tenant,
      std::vector<std::unique_ptr<RGWOIDCProvider>>& providers) override;
  virtual std::unique_ptr<Writer> get_append_writer(
      const DoutPrefixProvider* dpp, optional_yield y,
      std::unique_ptr<rgw::sal::Object> _head_obj, const rgw_user& owner,
      const rgw_placement_rule* ptail_placement_rule,
      const std::string& unique_tag, uint64_t position,
      uint64_t* cur_accounted_size) override;
  virtual std::unique_ptr<Writer> get_atomic_writer(
      const DoutPrefixProvider* dpp, optional_yield y,
      std::unique_ptr<rgw::sal::Object> _head_obj, const rgw_user& owner,
      const rgw_placement_rule* ptail_placement_rule, uint64_t olh_epoch,
      const std::string& unique_tag) override;
  virtual const std::string& get_compression_type(
      const rgw_placement_rule& rule) override;
  virtual bool valid_placement(const rgw_placement_rule& rule) override;

  virtual void finalize(void) override;

  virtual CephContext* ctx(void) override { return cctx; }

  virtual const std::string& get_luarocks_path() const override {
    return luarocks_path;
  }

  virtual void set_luarocks_path(const std::string& path) override {
    luarocks_path = path;
  }

  virtual int initialize(CephContext* cct,
                         const DoutPrefixProvider* dpp) override;
};

}  // namespace rgw::sal
