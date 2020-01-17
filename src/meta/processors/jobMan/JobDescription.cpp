/* Copyright (c) 2019 vesoft inc. All rights reserved.
 *
 * This source code is licensed under Apache 2.0 License,
 * attached with Common Clause Condition 1.0, found in the LICENSES directory.
 */

#include <stdexcept>
#include <string>
#include <vector>
#include <folly/String.h>
#include <boost/stacktrace.hpp>
#include "meta/processors/jobMan/JobUtils.h"
#include "meta/processors/jobMan/JobDescription.h"

#include "kvstore/KVIterator.h"
namespace nebula {
namespace meta {

JobDescription::JobDescription(int32_t id,
                               std::string type,
                               std::vector<std::string> paras,
                               JobStatus::Status status,
                               std::time_t startTime,
                               std::time_t stopTime)
                               : id_(id),
                                 type_(std::move(type)),
                                 paras_(std::move(paras)),
                                 status_(status),
                                 startTime_(startTime),
                                 stopTime_(stopTime) {}

folly::Optional<JobDescription>
JobDescription::makeJobDescription(folly::StringPiece rawkey,
                                   folly::StringPiece rawval) {
    try {
        auto key = parseKey(rawkey);
        LOG(INFO) << "after parseKey(rawkey);";

        auto tup = parseVal(rawval);
        LOG(INFO) << "after parseVal(rawval);";
        auto type = std::get<0>(tup);
        LOG(INFO) << "type = " << type;

        auto paras = std::get<1>(tup);
        for (auto p : paras) {
            LOG(INFO) << "p = " << p;
        }

        auto status = std::get<2>(tup);
        auto startTime = std::get<3>(tup);
        auto stopTime = std::get<4>(tup);
        return JobDescription(key, type, paras, status, startTime, stopTime);
    } catch(std::exception& ex) {
        LOG(ERROR) << ex.what();
    }
    return folly::none;
}

std::string JobDescription::jobKey() const {
    return makeJobKey(id_);
}

std::string JobDescription::makeJobKey(int32_t iJob) {
    std::string str;
    str.reserve(32);
    str.append(reinterpret_cast<const char*>(JobUtil::jobPrefix().data()),
                                             JobUtil::jobPrefix().size());
    str.append(reinterpret_cast<const char*>(&iJob), sizeof(int32_t));
    return str;
}

int32_t JobDescription::parseKey(const folly::StringPiece& rawKey) {
    auto offset = JobUtil::jobPrefix().size();
    if (offset + sizeof(size_t) < rawKey.size()) {
        std::stringstream oss;
        oss << __func__ << ", offset=" << offset << ", rawKey.size()=" << rawKey.size();
        throw std::range_error(oss.str().c_str());
    }
    return *reinterpret_cast<const int32_t*>(rawKey.begin() + offset);
}

std::string JobDescription::jobVal() const {
    std::string str;
    auto typeLen = type_.length();
    auto paraSize = paras_.size();
    str.reserve(256);
    str.append(reinterpret_cast<const char*>(&typeLen), sizeof(size_t));
    str.append(reinterpret_cast<const char*>(type_.data()), type_.length());
    str.append(reinterpret_cast<const char*>(&paraSize), sizeof(size_t));
    for (auto& para : paras_) {
        auto len = para.length();
        str.append(reinterpret_cast<const char*>(&len), sizeof(len));
        str.append(reinterpret_cast<const char*>(&para[0]), len);
    }
    str.append(reinterpret_cast<const char*>(&status_), sizeof(JobStatus::Status));
    str.append(reinterpret_cast<const char*>(&startTime_), sizeof(std::time_t));
    str.append(reinterpret_cast<const char*>(&stopTime_), sizeof(std::time_t));
    return str;
}

std::tuple<std::string,
           std::vector<std::string>,
           JobStatus::Status,
           std::time_t,
           std::time_t>
JobDescription::parseVal(const folly::StringPiece& rawVal) {
    size_t offset = 0;

    std::string type = JobUtil::parseString(rawVal, offset);
    offset += sizeof(size_t) + type.length();

    std::vector<std::string> paras = JobUtil::parseStrVector(rawVal, &offset);

    auto status = JobUtil::parseFixedVal<JobStatus::Status>(rawVal, offset);
    offset += sizeof(JobStatus::Status);

    auto tStart = JobUtil::parseFixedVal<std::time_t>(rawVal, offset);
    offset += sizeof(std::time_t);

    auto tStop = JobUtil::parseFixedVal<std::time_t>(rawVal, offset);

    return std::make_tuple(type, paras, status, tStart, tStop);
}

cpp2::JobDetails JobDescription::toJobDetails() {
    cpp2::JobDetails ret;
    ret.set_id(std::to_string(id_));
    std::stringstream oss;
    oss << type_ << " ";
    for (auto& p : paras_) {
        oss << p << " ";
    }
    ret.set_typeAndParas(oss.str());
    ret.set_status(JobStatus::toString(status_));
    ret.set_startTime(JobUtil::strTimeT(startTime_));
    ret.set_stopTime(JobUtil::strTimeT(stopTime_));
    return ret;
}

std::string JobDescription::archiveKey() {
    std::string str;
    str.reserve(32);
    str.append(reinterpret_cast<const char*>(JobUtil::archivePrefix().data()),
                                             JobUtil::archivePrefix().size());
    str.append(reinterpret_cast<const char*>(&id_), sizeof(id_));
    return str;
}

bool JobDescription::setStatus(JobStatus::Status newStatus) {
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

bool JobDescription::isJobKey(const folly::StringPiece& rawKey) {
    return rawKey.size() == JobUtil::jobPrefix().length() + sizeof(int32_t);
}

folly::Optional<JobDescription>
JobDescription::loadJobDescription(int32_t iJob, nebula::kvstore::KVStore* kv) {
    auto key = makeJobKey(iJob);
    std::string val;
    auto rc = kv->get(0, 0, key, &val);
    if (rc != nebula::kvstore::SUCCEEDED) {
        return folly::none;
    }
    return makeJobDescription(key, val);
}

}  // namespace meta
}  // namespace nebula

