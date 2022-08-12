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
    if(event.getType() != Event::Type::SUBSCRIPTION_DATA)
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
        if (element.has("AGG_TRADE_ID")) {
          tradeID = std::stoll(element.getValue("AGG_TRADE_ID"));
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

static const vector<std::string> BINANCE_SPOT_SYMBOLS =
    {
        "BTC-BUSD", "BTC-USDT",   "ETH-BUSD",   "ETH-USDT",  "SOL-BUSD",  "SOL-USDT",  "DOGE-BUSD", "DOGE-USDT", "DOT-BUSD",
        "DOT-USDT", "MATIC-BUSD", "MATIC-USDT", "AVAX-BUSD", "AVAX-USDT", "SHIB-BUSD", "SHIB-USDT", "TRX-BUSD",  "TRX-USDT",
    };

static const vector<std::string> COINBASE_SYMBOLS =
    {
        "BTC-USD",   "BTC-USDT", "ETH-USD",  "ETH-USDT",  "SOL-USD",  "SOL-USDT",  "DOGE-USD",
        "DOGE-USDT", "DOT-USD",  "DOT-USDT", "MATIC-USD", "AVAX-USD", "AVAX-USDT", "SHIB-USD",
    };
static const vector<std::string> KRAKEN_SYMBOLS =
    {
        "BTC-USD", "BTC-USDT", "ETH-USD", "ETH-USDT", "SOL-USD", "DOGE-USD", "DOGE-USDT", "DOT-USD", "DOT-USDT", "MATIC-USD", "AVAX-USD", "SHIB-USD", "TRX-USD",
    };


static const vector<std::string> BINANCE_FUTURES_SYMBOLS = {
    "BTCBUSD", "BTCUSDT", "ETHBUSD",   "ETHUSDT",   "SOLBUSD",  "SOLUSDT",  "DOGEBUSD", "DOGEUSDT",
    "DOTBUSD", "DOTUSDT", "MATICBUSD", "MATICUSDT", "AVAXBUSD", "AVAXUSDT", "TRXBUSD",  "TRXUSDT",

};

static const vector<std::string> FTX_SYMBOLS =
    {
        "BTC-USD",   "BTC-USDT",  "BTC-PERP",  "ETH-USD",   "ETH-USDT", "ETH-PERP", "SOL-USD",   "SOL-USDT",   "SOL-PERP",
        "DOGE-USD",  "DOGE-USDT", "DOGE-PERP", "DOT-USD",   "DOT-USDT", "DOT-PERP", "MATIC-USD", "MATIC-PERP", "AVAX-USD",
        "AVAX-USDT", "AVAX-PERP", "SHIB-USD",  "SHIB-PERP", "TRX-USD",  "TRX-USDT", "TRX-PERP",
    };
int main(int argc, char** argv) {
  SessionOptions sessionOptions;
  SessionConfigs sessionConfigs;
  MyEventHandler eventHandler;
  TradeEventHandler teventHandler;
  EventDispatcher eventDispatcher(4);
  Session session(sessionOptions, sessionConfigs, &eventHandler, &eventDispatcher);
  Session sessionx(sessionOptions, sessionConfigs, &teventHandler, &eventDispatcher);
  std::vector<Subscription> subscriptionList,tsubscriptionList;
  for(auto v:BINANCE_FUTURES_SYMBOLS){
    subscriptionList.emplace_back("binance-usds-futures", v, "MARKET_DEPTH", "","BINANCE_FUTURES."+ v);
    tsubscriptionList.emplace_back("binance-usds-futures",v,"AGG_TRADE","","BINANCE_FUTURES." + v );
  }
  for(auto v:BINANCE_SPOT_SYMBOLS){
    subscriptionList.emplace_back("binance", v, "MARKET_DEPTH", "","BINANCE_SPOT."+v);
    tsubscriptionList.emplace_back("binance",v,"AGG_TRADE","","BINANCE_SPOT."+v );
  }
  for(auto v:FTX_SYMBOLS){
    subscriptionList.emplace_back("ftx", v, "MARKET_DEPTH", "","FTX." + v);
    tsubscriptionList.emplace_back("ftx",v,"TRADE","","FTX." + v );
  }
  for(auto v:COINBASE_SYMBOLS){
    subscriptionList.emplace_back("coinbase", v, "MARKET_DEPTH", "","COINBASE." + v);
    tsubscriptionList.emplace_back("coinbase",v,"TRADE","","COINBASE." + v );
  }
  for(auto v:KRAKEN_SYMBOLS){
    subscriptionList.emplace_back("kraken", v, "MARKET_DEPTH", "","KRAKEN."+v);
    tsubscriptionList.emplace_back("kraken",v,"TRADE","","KRAKEN." + v );
  }
  session.subscribe(subscriptionList);
  sessionx.subscribe(tsubscriptionList);
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
