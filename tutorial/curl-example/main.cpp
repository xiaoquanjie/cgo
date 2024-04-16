#include <iostream>
#include <cgo/thirdparty/cocurl/cocurl.h>

int main() {
    cgo::WaitGroup wg;
    for (int i = 0; i< 1; i++) {
        wg.Add(1);
        go gostack(1024*32) [&wg] {
            std::string url = "www.baidu.com";
            auto rsp = cocurl::Get(url);
            if (rsp) {
                if (rsp->Ok()) {
                    std::cout << "rsp:" << rsp->value << "\n";
                } else {
                    std::cout << "curl error:" << rsp->curl_code << ":" << rsp->status_code << "\n";
                }
            } else {
                std::cout << "happend error\n";
            }
            wg.Done();
        };
    }

    wg.Wait();
    return 0;
}
