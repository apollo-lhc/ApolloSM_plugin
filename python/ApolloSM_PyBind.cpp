/*
 * Python binding for the ApolloSM class using pybind11 library.
 */

#include <ApolloSM/ApolloSM.hh>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(ApolloSM, m) {
    py::class_<IPBusIO>(m, "IPBusIO")
        .def(py::init<std::shared_ptr<uhal::HwInterface>>())
        .def("ReadAddress",            &IPBusIO::ReadAddress)
        .def("ReadRegister",           &IPBusIO::ReadRegister)
        .def("GetRegAddress",          &IPBusIO::GetRegAddress)
        .def("GetRegMask",             &IPBusIO::GetRegMask)
        .def("GetRegSize",             &IPBusIO::GetRegSize)
        .def("WriteAddress",           &IPBusIO::WriteAddress)
        .def("WriteRegister",          &IPBusIO::WriteRegister);

    py::class_<ApolloSM, IPBusIO>(m, "ApolloSM")
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