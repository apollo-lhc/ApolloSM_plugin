/*
 * Python binding for the ApolloSM class using pybind11 library.
 */

#include <ApolloSM/ApolloSM.hh>

#include <pybind11/pybind11.h>

namespace py = pybind11;

PYBIND11_MODULE(ApolloSM, m) {
    py::class_<ApolloSM>(m, "ApolloSM")
        .def(py::init<std::vector<std::string> const &>())
        .def("GenerateStatusDisplay",  &ApolloSM::GenerateStatusDisplay)
        .def("GenerateHTMLStatus",     &ApolloSM::GenerateHTMLStatus)
        .def("GenerateGraphiteStatus", &ApolloSM::GenerateGraphiteStatus)
        .def("UART_Terminal",          &ApolloSM::UART_Terminal)
        .def("UART_CMD",               &ApolloSM::UART_CMD)
        .def("svfplayer",              &ApolloSM::svfplayer)
        .def("PowerUpCM",              &ApolloSM::PowerUpCM)
        .def("PowerDownCM",            &ApolloSM::PowerDownCM)
        .def("DebugDump",              &ApolloSM::DebugDump)
        .def("unblockAXI",             &ApolloSM::unblockAXI)
        .def("restartCMuC",            &ApolloSM::restartCMuC)
        .def("GetSerialNumber",        &ApolloSM::GetSerialNumber)
        .def("GetRevNumber",           &ApolloSM::GetRevNumber)
        .def("GetShelfID",             &ApolloSM::GetShelfID)
        .def("GetSlot",                &ApolloSM::GetSlot)
        .def("GetZynqIP",              &ApolloSM::GetZynqIP)
        .def("GetIPMCIP",              &ApolloSM::GetIPMCIP);
}