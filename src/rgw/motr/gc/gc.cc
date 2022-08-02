// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=2 sw=2 expandtab ft=cpp

/*
 * Garbage Collector implementation for the CORTX Motr backend
 *
 * Copyright (C) 2022 Seagate Technology LLC and/or its Affiliates
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation. See file COPYING.
 *
 */

#include "motr/gc/gc.h"
#include <ctime>

void *MotrGC::GCWorker::entry() {
  std::unique_lock<std::mutex> lk(lock);
  ldpp_dout(dpp, 10) << __func__ << ": " << gc_thread_prefix
    << worker_id << " started." << dendl;

  // Get random number to lock the GC index.
  uint32_t my_index = ceph::util::generate_random_number(0, \
                      motr_gc->max_indices);
  // This is going to be endless loop
  do {
    ldpp_dout(dpp, 10) << __func__ << ": " << gc_thread_prefix
      << worker_id << " Iteration Started" << dendl;

    std::string iname = "";
    // Get lock on an GC index
    int rc = motr_gc->get_locked_gc_index(my_index);

    // Lock has been aquired, start the timer
    std::time_t start_time = std::time(nullptr);
    std::time_t end_time = start_time + \
			   cct->_conf->rgw_gc_processor_max_time - 10;
    std::time_t current_time = std::time(nullptr);

    if (rc == 0) {
      uint32_t processed_count = 0;
      // form the index name
      iname = gc_index_prefix + "." + std::to_string(my_index);
      ldpp_dout(dpp, 10) << __func__ << ": " << gc_thread_prefix
      << worker_id << " Working on GC Queue: " << iname << dendl;
      // time based while loop
      do {
        // fetch the next entry from index "iname"

        // check if the object is ready for deletion
        // for the motr object

        // delete that object
        // rc = dequeue();

        // remove the entry from iname

        // Exit the loop if required work is complete
        processed_count++;
        if (processed_count >= motr_gc->max_count) break;
        // Update current time
        current_time = std::time(nullptr);
      } while (current_time < end_time);
      // unlock the GC queue
    }
    my_index++;
    if (my_index >= motr_gc->max_indices) my_index = 0;
    // sleep for remaining duration
    if (end_time > current_time) sleep(end_time - current_time);
    cv.wait_for(lk, std::chrono::milliseconds(gc_interval * 10));

  } while (! motr_gc->going_down());

  ldpp_dout(dpp, 0) << __func__ << ": Stop signalled called for "
    << gc_thread_prefix << worker_id << dendl;
  return nullptr;
}

void MotrGC::initialize() {
  // fetch max gc indices from config
  uint32_t rgw_gc_max_objs = cct->_conf->rgw_gc_max_objs;
  if(rgw_gc_max_objs) {
    rgw_gc_max_objs = pow(2, ceil(log2(rgw_gc_max_objs)));
    max_indices = static_cast<int>(std::min(rgw_gc_max_objs,
                              GC_MAX_QUEUES));
  }
  else {
    max_indices = GC_DEFAULT_QUEUES;
  }
  index_names.reserve(max_indices);
  ldpp_dout(this, 50) << __func__ << ": max_indices = " << max_indices << dendl;
  for (uint32_t ind_suf = 0; ind_suf < max_indices; ind_suf++) {
    std::string iname = gc_index_prefix + "." + std::to_string(ind_suf);
    int rc = static_cast<rgw::sal::MotrStore*>(store)->create_motr_idx_by_name(iname);
    if (rc < 0 && rc != -EEXIST){
      ldout(cct, 0) << "ERROR: GC index creation failed with rc: " << rc << dendl;
      break;
    }
    index_names.push_back(iname);
  }
  // Get the max count of objects to be deleted in 1 processing cycle
  max_count = cct->_conf->rgw_gc_max_trim_chunk;
  if (max_count == 0) max_count = GC_DEFAULT_COUNT;
}

void MotrGC::finalize() {
  // We do not delete GC queues or related lock entry.
  // GC queue & lock entries would be needed after RGW / cluster restart,
  // so do not delete those.
}

void MotrGC::start_processor() {
  // fetch max_concurrent_io i.e. max_threads to create from config.
  // start all the gc_worker threads
  auto max_workers = cct->_conf->rgw_gc_max_concurrent_io;
  ldpp_dout(this, 50) << __func__ << ": max_workers = "
    << max_workers << dendl;
  workers.reserve(max_workers);
  for (int ix = 0; ix < max_workers; ++ix) {
    auto worker = std::make_unique<MotrGC::GCWorker>(this /* dpp */,
                                                     cct, this, ix);
    worker->create((gc_thread_prefix + std::to_string(ix)).c_str());
    workers.push_back(std::move(worker));
  }
}

void MotrGC::stop_processor() {
  // gracefully shutdown all the gc threads.
  down_flag = true;
  for (auto& worker : workers) {
    worker->stop();
    worker->join();
  }
  workers.clear();
}

void MotrGC::GCWorker::stop() {
  std::lock_guard l{lock};
  cv.notify_all();
}

bool MotrGC::going_down() {
  return down_flag;
}

int MotrGC::dequeue(const DoutPrefixProvider* dpp, std::string iname, motr_gc_obj_info obj)
{
  int rc;
  bufferlist bl;
  rc = static_cast<rgw::sal::MotrStore*>(store)->do_idx_op_by_name(iname,
                                M0_IC_DEL, obj.tag, bl);
  if (rc < 0){
    ldout(cct, 0) << "ERROR: failed to delete tag entry "<<obj.tag<<" rc: " << rc << dendl;
  }
  rc = static_cast<rgw::sal::MotrStore*>(store)->do_idx_op_by_name(iname,
                                M0_IC_DEL, std::to_string(obj.time), bl);
  if (rc < 0 && rc != -EEXIST){
    ldout(cct, 0) << "ERROR: failed to delete time entry "<<obj.time<<" rc: " << rc << dendl;
  }
  return rc;
}

int MotrGC::get_locked_gc_index(uint32_t& rand_ind)
{
  int rc = -1;
  uint32_t new_index = 0;
  // attempt to lock GC starting with passed in index
  for (uint32_t ind = 0; ind < max_indices; ind++)
  {
    new_index = (ind + rand_ind) % max_indices;
    // try locking index
    // on sucess mark rc as 0
  }
  rand_ind = new_index;
  return rc;
}

unsigned MotrGC::get_subsys() const {
  return dout_subsys;
}

std::ostream& MotrGC::gen_prefix(std::ostream& out) const {
  return out << "garbage_collector: ";
}
