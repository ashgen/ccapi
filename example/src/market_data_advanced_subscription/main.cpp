#include "ccapi_cpp/ccapi_session.h"
#include <reckless/policy_log.hpp>
#include "reckless/file_writer.hpp"
#include "reckless/severity_log.hpp"
using log_t = reckless::severity_log<reckless::indent<4>, ','>;
using namespace std;
static const string tradeLogFile = "logs/trade.csv";
static const string quoteLogFile = "logs/quote.csv";
static constexpr size_t QUEUE_SIZE = 4096 * 4096;

reckless::file_writer quote_writer(quoteLogFile.c_str());
log_t quote_logger(&quote_writer, 8 * QUEUE_SIZE,8*QUEUE_SIZE);

reckless::file_writer trade_writer(tradeLogFile.c_str());
log_t trade_logger(&trade_writer, QUEUE_SIZE,QUEUE_SIZE);


void stick_this_thread_to_core(std::thread& _thread,int core_id) {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);
  int rc=pthread_setaffinity_np(_thread.native_handle(), sizeof(cpu_set_t), &cpuset);
  if (rc != 0) {
    std::cerr << "Error calling pthread_setaffinity_np: " << rc << "\n";
  }
}


namespace ccapi {
Logger* Logger::logger = nullptr;  // This line is needed.
class MyEventHandler : public EventHandler {
 public:
  bool processEvent(const Event& event, Session* session) override {
  if(event.getType() != Event::Type::SUBSCRIPTION_DATA)
    return true;
    std::lock_guard<std::mutex> lock(m);
    for (const auto& message : event.getMessageList()) {
      auto correlationId = message.getCorrelationIdList().at(0);
      time = message.getTime().time_since_epoch().count();
      timeRecieved = message.getTimeReceived().time_since_epoch().count();
      for (const auto& element : message.getElementList()) {
        if(element.has("BID_PRICE")){
          bestBidPrice = std::stod(element.getValue("BID_PRICE"));
        }
        if (element.has("ASK_PRICE")) {
          bestAskPrice = std::stod(element.getValue("ASK_PRICE"));
        }
        if (element.has("BID_SIZE")) {
          bestBidSize = std::stod(element.getValue("BID_SIZE"));
        }
        if (element.has("ASK_SIZE")) {
          bestAskSize = std::stod(element.getValue("ASK_SIZE"));
        }
      }
      quote_logger.info("%d,%d,%s,%f,%f,%f,%f",timeRecieved,time,correlationId,bestBidPrice,bestAskPrice,bestBidSize,bestAskSize);
    }
    return true;
  }
 private:
  mutable std::mutex m;
  double bestBidPrice;
  double bestAskPrice;
  double bestBidSize;
  double bestAskSize;
  std::string symbol;
  uint64_t time,timeRecieved;
};
class TradeEventHandler : public EventHandler {
 public:
  bool processEvent(const Event& event, Session* session) override {
    if(event.getType() != Event::Type::RESPONSE)
      return true;
    std::lock_guard<std::mutex> lock(m);
    for (const auto& message : event.getMessageList()) {
      auto correlationId = message.getCorrelationIdList().at(0);
      time = message.getTime().time_since_epoch().count();
      timeRecieved = message.getTimeReceived().time_since_epoch().count();
      for (const auto& element : message.getElementList()) {
        if(element.has("LAST_PRICE")){
          price = std::stod(element.getValue("LAST_PRICE"));
        }
        if (element.has("LAST_SIZE")) {
          size = std::stod(element.getValue("LAST_SIZE"));
        }
        if (element.has("TRADE_ID")) {
          tradeID = std::stoll(element.getValue("TRADE_ID"));
        }
        if (element.has("IS_BUYER_MAKER")) {
          side = std::stoi(element.getValue("IS_BUYER_MAKER"));
        }
      }
      trade_logger.info("%d,%d,%s,%f,%f,%d,%d",timeRecieved,time,correlationId,price,size,side,tradeID);
    }
    return true;
  }
 private:
  mutable std::mutex m;
  double price;
  double size;
  int side;
  std::string symbol;
  uint64_t time,timeRecieved;
  long long tradeID;
};
} /* namespace ccapi */
using ::ccapi::Event;
using ::ccapi::EventDispatcher;
using ::ccapi::MyEventHandler;
using ::ccapi::TradeEventHandler;
using ::ccapi::Session;
using ::ccapi::SessionConfigs;
using ::ccapi::SessionOptions;
using ::ccapi::Subscription;
using ::ccapi::toString;
int main(int argc, char** argv) {
    SessionOptions sessionOptions;
    SessionConfigs sessionConfigs;
    MyEventHandler eventHandler;
    TradeEventHandler teventHandler;
    EventDispatcher eventDispatcher(4);
    Session session(sessionOptions, sessionConfigs, &eventHandler, &eventDispatcher);
    Session sessionx(sessionOptions, sessionConfigs, &teventHandler, &eventDispatcher);
    std::vector<Subscription> subscriptionList;
    subscriptionList.emplace_back("binance-usds-futures", "BTCUSDT", "MARKET_DEPTH", "", "1");
    subscriptionList.emplace_back("binance-usds-futures", "BTCBUSD", "MARKET_DEPTH", "", "2");
    session.subscribe(subscriptionList);

    std::vector<ccapi::Request> tsubscriptionList;
    ccapi::Request r1(ccapi::Request::Operation::GET_RECENT_TRADES,"binance-usds-futures", "BTCUSDT","3");
    r1.appendParam({{"limit","1"}});
    ccapi::Request r2(ccapi::Request::Operation::GET_RECENT_TRADES,"binance-usds-futures", "BTCBUSD","4");
    r2.appendParam({{"limit","1"}});
    tsubscriptionList.emplace_back(r1);
    tsubscriptionList.emplace_back(r2);
    sessionx.sendRequest(tsubscriptionList);
    quote_logger.info("timeRecieved,exchangeTime,symbol,bidPrice,askPrice,bidSize,askSize");
    trade_logger.info("timeRecieved,exchangeTime,symbol,price,size,side,tradeID");

    while(true){

    }
    session.stop();
    sessionx.stop();
    eventDispatcher.stop();

    std::cout << "Bye" << std::endl;
  return EXIT_SUCCESS;
}
