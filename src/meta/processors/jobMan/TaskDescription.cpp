/* Copyright (c) 2019 vesoft inc. All rights reserved.
 *
 * This source code is licensed under Apache 2.0 License,
 * attached with Common Clause Condition 1.0, found in the LICENSES directory.
 */

#include "folly/String.h"
#include "meta/processors/jobMan/TaskDescription.h"
#include "meta/processors/jobMan/JobStatus.h"
#include "meta/processors/jobMan/JobUtils.h"

namespace nebula {
namespace meta {

TaskDescription::TaskDescription(int32_t iJob,
                                 int32_t iTask,
                                 const std::string& dest)
                                 : iJob_(iJob),
                                   iTask_(iTask),
                                   dest_(dest),
                                   status_(JobStatus::Status::RUNNING),
                                   startTime_(std::time(nullptr)),
                                   stopTime_(0) {}


/*
 * int32_t                         iJob_;
 * int32_t                         iTask_;
 * std::string                     dest_;
 * JobStatus::Status               status_;
 * std::time_t                     startTime_;
 * std::time_t                     stopTime_;
 * */
TaskDescription::TaskDescription(const folly::StringPiece& key,
                                 const folly::StringPiece& val) {
    auto tupKey = parseKey(key);
    iJob_ = std::get<0>(tupKey);
    iTask_ = std::get<1>(tupKey);

    auto tupVal = parseVal(val);
    dest_ = std::get<0>(tupVal);
    status_ = std::get<1>(tupVal);
    startTime_ = std::get<2>(tupVal);
    stopTime_ = std::get<3>(tupVal);
}

std::string TaskDescription::taskKey() {
    std::string str;
    str.reserve(32);
    str.append(reinterpret_cast<const char*>(JobUtil::jobPrefix().data()),
                                             JobUtil::jobPrefix().size());
    str.append(reinterpret_cast<const char*>(&iJob_), sizeof(iJob_));
    str.append(reinterpret_cast<const char*>(&iTask_), sizeof(iTask_));
    return str;
}

std::tuple<int32_t, int32_t>
TaskDescription::parseKey(const folly::StringPiece& rawKey) {
    auto offset = JobUtil::jobPrefix().size();
    int32_t iJob =  *reinterpret_cast<const int32_t*>(rawKey.begin() + offset);
    offset += sizeof(int32_t);
    int32_t iTask = *reinterpret_cast<const int32_t*>(rawKey.begin() + offset);
    return std::make_tuple(iJob, iTask);
}

std::string TaskDescription::archiveKey() {
    std::string str;
    str.reserve(32);
    str.append(reinterpret_cast<const char*>(JobUtil::archivePrefix().data()),
                                             JobUtil::archivePrefix().size());
    str.append(reinterpret_cast<const char*>(&iJob_), sizeof(iJob_));
    str.append(reinterpret_cast<const char*>(&iTask_), sizeof(iTask_));
    return str;
}

std::string TaskDescription::taskVal() {
    std::string str;
    str.reserve(128);
    auto destLen = dest_.length();
    str.append(reinterpret_cast<const char*>(&destLen), sizeof(destLen));
    str.append(reinterpret_cast<const char*>(&dest_[0]), dest_.length());
    str.append(reinterpret_cast<const char*>(&status_), sizeof(JobStatus::Status));
    str.append(reinterpret_cast<const char*>(&startTime_), sizeof(std::time_t));
    str.append(reinterpret_cast<const char*>(&stopTime_), sizeof(std::time_t));
    return str;
}

/*
 *  std::string                     dest_;
 *  JobStatus::Status               status_;
 *  std::time_t                     startTime_;
 *  std::time_t                     stopTime_;
 * */
std::tuple<std::string, JobStatus::Status,
           std::time_t,
           std::time_t>
TaskDescription::parseVal(const folly::StringPiece& rawVal) {
    size_t offset = 0;

    std::string host = JobUtil::parseString(rawVal, offset);
    offset += sizeof(size_t) + host.length();

    auto status = JobUtil::parseFixedVal<JobStatus::Status>(rawVal, offset);
    offset += sizeof(JobStatus::Status);

    auto tStart = JobUtil::parseFixedVal<std::time_t>(rawVal, offset);
    offset += sizeof(std::time_t);

    auto tStop = JobUtil::parseFixedVal<std::time_t>(rawVal, offset);

    return std::make_tuple(host, status, tStart, tStop);
}

/*
 * =====================================================================================
 * | Job Id(TaskId) | Command(Dest) | Status   | Start Time        | Stop Time         |
 * =====================================================================================
 * | 27-0           | 192.168.8.5   | finished | 12/09/19 11:09:40 | 12/09/19 11:09:40 |
 * -------------------------------------------------------------------------------------
 * */
cpp2::TaskDetails TaskDescription::toTaskDetails() {
    cpp2::TaskDetails ret;
    ret.set_id(folly::stringPrintf("%d-%d", iJob_, iTask_));
    ret.set_host(dest_);
    ret.set_status(JobStatus::toString(status_));
    ret.set_startTime(JobUtil::strTimeT(startTime_));
    ret.set_stopTime(JobUtil::strTimeT(stopTime_));
    return ret;
}

bool TaskDescription::setStatus(JobStatus::Status newStatus) {
    if (JobStatus::laterThan(status_, newStatus)) {
        return false;
    }
    status_ = newStatus;
    if (newStatus == JobStatus::Status::RUNNING) {
        startTime_ = std::time(nullptr);
    }

    if (JobStatus::laterThan(newStatus, JobStatus::Status::RUNNING)) {
        stopTime_ = std::time(nullptr);
    }
    return true;
}

}  // namespace meta
}  // namespace nebula

