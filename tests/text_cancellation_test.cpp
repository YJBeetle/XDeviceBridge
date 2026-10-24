#include "hid/TextReports.h"
#include "transport/IoBudget.h"
#include <future>
#include <iostream>
#include <stdexcept>
using namespace std::chrono_literals;
void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
int main() {
    using xdb::hid::detail::send_text_reports;
    const std::vector<std::vector<uint16_t>> reports{{4}, {}, {5}, {}};
    std::string error;
    std::stop_source stopped; stopped.request_stop();
    unsigned sent = 0;
    require(!send_text_reports(reports, 0, stopped.get_token(), [&](auto &) { ++sent; return true; }, error)
            && sent == 0, "already cancelled sequence delivered a report");
    std::stop_source stop;
    std::promise<void> first;
    auto ready = first.get_future();
    auto result = std::async(std::launch::async, [&] {
        unsigned count = 0;
        std::string why;
        bool ok = send_text_reports(reports, 1000, stop.get_token(), [&](auto &) {
            if (++count == 1) first.set_value();
            return true;
        }, why);
        return !ok && count == 1 && !why.empty();
    });
    require(ready.wait_for(1s) == std::future_status::ready, "first report did not arrive");
    stop.request_stop();
    require(result.wait_for(500ms) == std::future_status::ready && result.get(),
            "cancellation did not wake the one-second hold or sent later reports");
    std::stop_source deadline_stop;
    xdb::transport::IoBudget budget({25ms, {}});
    xdb::transport::BudgetWatchdog deadline_watch(budget, [&] { deadline_stop.request_stop(); });
    sent = 0;
    const auto started = std::chrono::steady_clock::now();
    require(!send_text_reports(reports, 1000, deadline_stop.get_token(),
                [&](auto &) { ++sent; return true; }, error), "deadline did not stop text");
    require(!deadline_watch.finish(error) && sent == 1 &&
                std::chrono::steady_clock::now() - started < 500ms,
            "budget failed to interrupt the hold promptly");
    std::vector<std::vector<uint16_t>> observed;
    require(send_text_reports(reports, 0, {}, [&](auto &r) { observed.push_back(r); return true; }, error)
            && observed == reports, "uncancelled sequence changed report order or final release");
    sent = 0;
    require(!send_text_reports(reports, 0, {}, [&](auto &) { return ++sent < 2; }, error)
            && sent == 2, "send failure continued typing");
    std::cout << "PASS: stopped before delivery, cancellation wakes hold, order/release and send failure\n";
}
