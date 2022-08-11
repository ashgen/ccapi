#include "ccapi_cpp/ccapi_session.h"
namespace ccapi {
Logger* Logger::logger = nullptr;  // This line is needed.
class MyEventHandler : public EventHandler {
 public:
  bool processEvent(const Event& event, Session* session) override {
    std::cout << toString(event) + "\n" << std::endl;
    return true;
  }
};
class TradeEventHandler : public EventHandler {
 public:
  bool processEvent(const Event& event, Session* session) override {
    std::cout << toString(event) + "\n" << std::endl;
    return true;
  }
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
  std::vector<std::string> modeList = {
      "dispatch_events_to_multiple_threads",
      "handle_events_in_batching_mode",
  };
  if (argc != 2 || std::find(modeList.begin(), modeList.end(), argv[1]) == modeList.end()) {
    std::cerr << "Please provide one command line argument from this list: " + toString(modeList) << std::endl;
    return EXIT_FAILURE;
  }
  std::string mode(argv[1]);
  if (mode == "dispatch_events_to_multiple_threads") {
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

    std::this_thread::sleep_for(std::chrono::seconds(1000));
    session.stop();
    sessionx.stop();
    eventDispatcher.stop();
  } else if (mode == "handle_events_in_batching_mode") {
    SessionOptions sessionOptions;
    SessionConfigs sessionConfigs;
    Session session(sessionOptions, sessionConfigs);
    Subscription subscription("coinbase", "BTC-USD", "MARKET_DEPTH");
    session.subscribe(subscription);
    std::this_thread::sleep_for(std::chrono::seconds(10));
    std::vector<Event> eventList = session.getEventQueue().purge();
    for (const auto& event : eventList) {
      std::cout << toString(event) + "\n" << std::endl;
    }
    session.stop();
  }
  std::cout << "Bye" << std::endl;
  return EXIT_SUCCESS;
}
