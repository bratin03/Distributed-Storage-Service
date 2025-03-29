#include <boost/log/trivial.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <iostream>

// Initialize the logging framework
void init_logging()
{
    // Configure file sink: set file name and output format
    boost::log::add_file_log(
        boost::log::keywords::file_name = "sample.log",
        boost::log::keywords::format = "[%TimeStamp%] [%Severity%]: %Message%");
    // Set filter to log only messages with severity >= info
    boost::log::core::get()->set_filter(
        boost::log::trivial::severity >= boost::log::trivial::info);
    // Add common attributes like TimeStamp, ProcessID, ThreadID, etc.
    boost::log::add_common_attributes();
}

int main()
{
    init_logging();

    // These messages use different severity levels.
    // Only messages with severity info or higher will be logged.
    BOOST_LOG_TRIVIAL(trace) << "This trace message will be filtered out";
    BOOST_LOG_TRIVIAL(debug) << "This debug message will be filtered out";
    BOOST_LOG_TRIVIAL(info) << "This is an informational message";
    BOOST_LOG_TRIVIAL(warning) << "This is a warning message";
    BOOST_LOG_TRIVIAL(error) << "This is an error message";
    BOOST_LOG_TRIVIAL(fatal) << "This is a fatal message";

    std::cout << "Logging complete. Check sample.log for output." << std::endl;
    return 0;
}
