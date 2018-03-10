/*
 * Copyright(c) 2018, SEQSENSE, Inc.
 * All rights reserved.
 */

#ifndef SCIP_RESPONSE_STREAM_H
#define SCIP_RESPONSE_STREAM_H

#include <boost/asio.hpp>

#include <string>
#include <map>

#include <scip/response/abstract.h>
#include <scip/decode.h>

namespace scip
{
class ScanData
{
public:
  uint32_t timestamp_;
  std::vector<int32_t> ranges_;
  std::vector<int32_t> intensities_;
};

class ResponseStream : public Response
{
public:
  using Callback = boost::function<void(
      const std::string &, const std::string &, const ScanData &)>;

protected:
  Callback cb_;

public:
  virtual std::string getCommandCode() const = 0;
  virtual void operator()(
      const std::string &echo_back,
      const std::string &status,
      std::istream &stream) = 0;

  bool readTimestamp(
      const std::string &echo_back,
      const std::string &status,
      std::istream &stream,
      ScanData &scan)
  {
    std::map<std::string, std::string> params;
    if (status == "00")
    {
      return false;
    }
    if (status != "99")
    {
      if (cb_)
        cb_(echo_back, status, scan);
      std::cout << echo_back << " errored with " << status << std::endl;
      return false;
    }
    std::string stamp;
    if (!std::getline(stream, stamp))
    {
      std::cerr << "Failed to get timestamp" << std::endl;
      return false;
    }
    stamp.pop_back();  // remove checksum
    if (stamp.size() < 4)
    {
      std::cerr << "Wrong timestamp format" << std::endl;
      return false;
    }

    auto dec = Decoder<4>(stamp);
    scan.timestamp_ = *dec.begin();
    return true;
  }
  void registerCallback(Callback cb)
  {
    cb_ = cb;
  }
};

class ResponseMD : public ResponseStream
{
public:
  std::string getCommandCode() const
  {
    return std::string("MD");
  }
  void operator()(
      const std::string &echo_back,
      const std::string &status,
      std::istream &stream) override
  {
    ScanData scan;
    if (!readTimestamp(echo_back, status, stream, scan))
      return;

    std::string line;
    scip::DecoderRemain remain;
    while (std::getline(stream, line))
    {
      if (line.size() == 0)
        break;

      line.pop_back();  // remove checksum
      if (line.size() < 3)
      {
        std::cerr << "Wrong stream format" << std::endl;
        return;
      }
      auto dec = Decoder<3>(line, remain);
      auto it = dec.begin();
      for (; it != dec.end(); ++it)
      {
        scan.ranges_.push_back(*it);
      }
      remain = it.getRemain();
    }
    if (cb_)
      cb_(echo_back, status, scan);
  }
};

class ResponseME : public ResponseStream
{
public:
  std::string getCommandCode() const
  {
    return std::string("ME");
  }
  void operator()(
      const std::string &echo_back,
      const std::string &status,
      std::istream &stream) override
  {
    ScanData scan;
    if (!readTimestamp(echo_back, status, stream, scan))
      return;

    std::string line;
    scip::DecoderRemain remain;
    while (std::getline(stream, line))
    {
      if (line.size() == 0)
        break;

      line.pop_back();  // remove checksum
      if (line.size() < 3)
      {
        std::cerr << "Wrong stream format" << std::endl;
        return;
      }
      auto dec = Decoder<3>(line, remain);
      auto it = dec.begin();
      bool range(true);
      for (; it != dec.end(); ++it)
      {
        if (range)
          scan.ranges_.push_back(*it);
        else
          scan.intensities_.push_back(*it);
        range = !range;
      }
      remain = it.getRemain();
    }
    if (cb_)
      cb_(echo_back, status, scan);
  }
};

}  // namespace scip

#endif  // RESPONSE_H
