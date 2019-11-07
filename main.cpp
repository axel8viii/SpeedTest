#include <iostream>
#include <map>
#include <iomanip>
#include "SpeedTest.h"
#include "TestConfigTemplate.h"
#include "CmdOptions.h"
#include <csignal>

void banner(){
    std::cout << "SpeedTest++ version " << SpeedTest_VERSION_MAJOR << "." << SpeedTest_VERSION_MINOR << std::endl;
    std::cout << "Speedtest.net command line interface" << std::endl;
    std::cout << "Info: " << SpeedTest_HOME_PAGE << std::endl;
    std::cout << "Author: " << SpeedTest_AUTHOR << std::endl;
}

void usage(const char* name){
    std::cerr << "Usage: " << name << " ";
    std::cerr << " [--latency] [--quality] [--download] [--upload] [--share] [--help]\n"
            "      [--test-server host:port] [--output verbose|text|json]\n";
    std::cerr << "optional arguments:" << std::endl;
    std::cerr << "  --help                      Show this message and exit\n";
    std::cerr << "  --latency                   Perform latency test only\n";
    std::cerr << "  --download                  Perform download test only. It includes latency test\n";
    std::cerr << "  --upload                    Perform upload test only. It includes latency test\n";
    std::cerr << "  --share                     Generate and provide a URL to the speedtest.net share results image\n";
    std::cerr << "  --test-server host:port     Run speed test against a specific server\n";
    std::cerr << "  --output verbose|text|json  Set output type. Default: verbose\n";
}

int main(const int argc, const char **argv) {

    ProgramOptions programOptions;

    if (!ParseOptions(argc, argv, programOptions)){
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (programOptions.output_type == OutputType::verbose){
        banner();
        std::cout << std::endl;
    }


    if (programOptions.help) {
        usage(argv[0]);
        return EXIT_SUCCESS;
    }

    signal(SIGPIPE, SIG_IGN);


    auto sp = SpeedTest(SPEED_TEST_MIN_SERVER_VERSION);
    IPInfo info;
    ServerInfo serverInfo;
    ServerInfo serverQualityInfo;

    if (!sp.ipInfo(info)){
        std::cerr << "Unable to retrieve your IP info. Try again later" << std::endl;
        return EXIT_FAILURE;
    }

    if (programOptions.output_type == OutputType::verbose){
        std::cout << "IP: " << info.ip_address
                  << " ( " << info.isp << " ) "
                  << "Location: [" << info.lat << ", " << info.lon << "]" << std::endl;
    } else if (programOptions.output_type == OutputType::json) {
        std::cout << "{"
                  << "\"client\": {"
//                  << "\"isprating\": \"3.7\""
                  << "\"ip\": \"" << info.ip_address << "\""
                  << ", \"isp\": \"" << info.isp << "\""
                  << ", \"lon\": \"" << info.lat << "\""
                  << ", \"lat\": \"" << info.lat << "\""
                  << "}";
    } else {
        std::cout << "IP=" << info.ip_address << std::endl;
        std::cout << "IP_LAT=" << info.lat << std::endl;
        std::cout << "IP_LON=" << info.lon << std::endl;
        std::cout << "PROVIDER=" << info.isp << std::endl;
    }

    auto serverList = sp.serverList();

    if (programOptions.selected_server.empty()){
        if (programOptions.output_type == OutputType::verbose)
            std::cout << "Finding fastest server... " << std::flush;
        
        if (serverList.empty()){
            std::cerr << "Unable to download server list. Try again later" << std::endl;
            return EXIT_FAILURE;
        }

        if (programOptions.output_type == OutputType::verbose)
            std::cout << serverList.size() << " Servers online" << std::endl;


        serverInfo = sp.bestServer(10, [&programOptions](bool success) {
            if (programOptions.output_type == OutputType::verbose)
                std::cout << (success ? '.' : '*') << std::flush;
        });

        if (programOptions.output_type == OutputType::verbose){
            std::cout << std::endl;
            std::cout << "Server: " << serverInfo.name
                      << " " << serverInfo.host
                      << " by " << serverInfo.sponsor
                      << " (" << serverInfo.distance << " km from you): "
                      << std::setprecision(1) << std::fixed << sp.latency()/1000.0 << " ms" << std::endl;
        } else if (programOptions.output_type == OutputType::json) {
            std::cout << ", \"server\": {"
                      << "\"name\": \"" << serverInfo.name << "\""
                      << ", \"url\": \"" << serverInfo.url << "\""
                      << ", \"country\": \"" << serverInfo.country << "\""
                      << ", \"lon\": \"" << serverInfo.lon << "\""
                      << ", \"cc\": \"" << serverInfo.country_code << "\""
                      << ", \"host\": \"" << serverInfo.host << "\""
                      << ", \"sponsor\": \"" << serverInfo.sponsor << "\""
//                      << ", \"url2\": \"" << serverInfo.url2 << "\""
                      << ", \"lat\": \"" << serverInfo.lat << "\""
                      << ", \"id\": \"" << serverInfo.id << "\""
                      << ", \"d\": " << std::fixed << serverInfo.distance
                      << ", \"latency\": " << std::setprecision(1) << std::fixed << sp.latency()/1000.0
                      << "}";
        } else {
            std::cout << "TEST_SERVER_HOST=" << serverInfo.host << std::endl;
            std::cout << "TEST_SERVER_DISTANCE=" << serverInfo.distance << std::endl;

        }

    } else {

        serverInfo.host.append(programOptions.selected_server);
        sp.setServer(serverInfo);

        for (auto &s : serverList) {
            if (s.host == serverInfo.host)
                serverInfo.id = s.id;
        }

        if (programOptions.output_type == OutputType::verbose)
            std::cout << "Selected server: " << serverInfo.host << std::endl;
        else if (programOptions.output_type == OutputType::text)
            std::cout << "TEST_SERVER_HOST=" << serverInfo.host << std::endl;
    }

    if (programOptions.output_type == OutputType::verbose)
        std::cout << "Ping: " << std::setprecision(1) << std::fixed << sp.latency()/1000.0 << " ms." << std::endl;
    else if (programOptions.output_type == OutputType::json)
        std::cout << ", \"ping\": " << std::setprecision(1) << std::fixed << sp.latency()/1000.0;
    else
        std::cout << "LATENCY=" << std::setprecision(1) << std::fixed << sp.latency()/1000.0 << std::endl;

    long jitter = 0;
    if (programOptions.output_type == OutputType::verbose)
        std::cout << "Jitter: " << std::flush;
    if (sp.jitter(serverInfo, jitter)){
        if (programOptions.output_type == OutputType::verbose)
            std::cout << std::setprecision(1) << std::fixed << jitter/1000.0 << " ms." << std::endl;
        else if (programOptions.output_type == OutputType::json)
            std::cout << ", \"jitter\": " << std::setprecision(1) << std::fixed << jitter/1000.0;
        else
            std::cout << "JITTER=" << std::setprecision(1) << std::fixed << jitter/1000.0 << std::endl;
    } else {
        std::cerr << "Jitter measurement is unavailable at this time." << std::endl;
    }

    if (programOptions.latency) {
        if (programOptions.output_type == OutputType::json)
            std::cout << "}" << std::endl;
        return EXIT_SUCCESS;
    }

    if (programOptions.output_type == OutputType::verbose)
        std::cout << "Determine line type (" << preflightConfigDownload.concurrency << ") "  << std::flush;
    double preSpeed = 0;
    long preSize = 0;
    if (!sp.downloadSpeed(serverInfo, preflightConfigDownload, preSpeed, preSize, [&programOptions](bool success){
        if (programOptions.output_type == OutputType::verbose)
            std::cout << (success ? '.' : '*') << std::flush;
    })){
        std::cerr << "Pre-flight check failed." << std::endl;
        return EXIT_FAILURE;
    }

    if (programOptions.output_type == OutputType::verbose)
        std::cout << std::endl;

    TestConfig uploadConfig;
    TestConfig downloadConfig;
    testConfigSelector(preSpeed, uploadConfig, downloadConfig);

    if (programOptions.output_type == OutputType::verbose)
        std::cout << downloadConfig.label << std::endl;


    if (!programOptions.upload){
        if (programOptions.output_type == OutputType::verbose){
            std::cout << std::endl;
            std::cout << "Testing download speed (" << downloadConfig.concurrency << ") "  << std::flush;
        }

        double downloadSpeed = 0;
        long downloadSize = 0;
        if (sp.downloadSpeed(serverInfo, downloadConfig, downloadSpeed, downloadSize, [&programOptions](bool success){
            if (programOptions.output_type == OutputType::verbose)
                std::cout << (success ? '.' : '*') << std::flush;
        })){
            if (programOptions.output_type == OutputType::verbose){
                std::cout << std::endl;
                std::cout << "Download: ";
                std::cout << std::fixed;
                std::cout << std::setprecision(2);
                std::cout << downloadSpeed << " Mbit/s" << std::endl;
            } if (programOptions.output_type == OutputType::json) {
                std::cout << ", \"download\": " << std::fixed << downloadSpeed * 1000 * 1000;
                std::cout << ", \"bytes_received\": " << std::fixed << downloadSize;
            } else {
                std::cout << "DOWNLOAD_SPEED=";
                std::cout << std::fixed;
                std::cout << std::setprecision(2);
                std::cout << downloadSpeed << std::endl;
            }

        } else {
            std::cerr << "Download test failed." << std::endl;
            return EXIT_FAILURE;
        }
    }

    if (programOptions.download) {
        if (programOptions.output_type == OutputType::json)
            std::cout << "}" << std::endl;
        return EXIT_SUCCESS;
    }

    if (programOptions.output_type == OutputType::verbose)
        std::cout << "Testing upload speed (" << uploadConfig.concurrency << ") "  << std::flush;

    double uploadSpeed = 0;
    long uploadSize = 0;
    if (sp.uploadSpeed(serverInfo, uploadConfig, uploadSpeed, uploadSize, [&programOptions](bool success){
        if (programOptions.output_type == OutputType::verbose)
            std::cout << (success ? '.' : '*') << std::flush;
    })){
        if (programOptions.output_type == OutputType::verbose){
            std::cout << std::endl;
            std::cout << "Upload: ";
            std::cout << std::fixed;
            std::cout << std::setprecision(2);
            std::cout << uploadSpeed << " Mbit/s" << std::endl;
        } if (programOptions.output_type == OutputType::json) {
            std::cout << ", \"upload\": " << std::fixed << uploadSpeed * 1000 * 1000;
            std::cout << ", \"bytes_sent\": " << std::fixed << uploadSize;
        } else {
            std::cout << "UPLOAD_SPEED=";
            std::cout << std::fixed;
            std::cout << std::setprecision(2);
            std::cout << uploadSpeed << std::endl;
        }

    } else {
        std::cerr << "Upload test failed." << std::endl;
        return EXIT_FAILURE;
    }


    if (programOptions.share){
        std::string share_it;
        if (sp.share(serverInfo, share_it)) {
            if (programOptions.output_type == OutputType::verbose) {
                std::cout << "Results image: " << share_it << std::endl;
            } else if (programOptions.output_type == OutputType::json) {
                std::cout << ", \"share\": \"" << share_it << "\"";
            } else {
                std::cout << "IMAGE_URL=" << share_it << std::endl;
            }
        }
    }

    if (programOptions.output_type == OutputType::json)
        std::cout << "}" << std::endl;

    return EXIT_SUCCESS;
}
