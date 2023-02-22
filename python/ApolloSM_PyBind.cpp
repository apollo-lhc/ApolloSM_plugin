/*
 * Python binding for the ApolloSM class using pybind11 library.
 */

#include <ApolloSM/ApolloSM.hh>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

/*
 * The following defines the bindings for the IPBusConnection, IPBusIO 
 * and ApolloSM classes. 
 * The inheritance is set up such that ApolloSM class inherits from
 * the IPBusConnection and IPBusIO classes.
 */
PYBIND11_MODULE(ApolloSM, m) {
    py::class_<IPBusConnection>(m, "IPBusConnection")
        .def(py::init<std::string const &, std::vector<std::string> const &>())
        .def("Connect",                &IPBusConnection::Connect);

    py::class_<IPBusIO>(m, "IPBusIO")
        .def(py::init<std::shared_ptr<uhal::HwInterface>>())
        .def("ReadAddress",            &IPBusIO::ReadAddress)
        .def("ReadRegister",           &IPBusIO::ReadRegister)
        .def("GetRegAddress",          &IPBusIO::GetRegAddress)
        .def("GetRegMask",             &IPBusIO::GetRegMask)
        .def("GetRegSize",             &IPBusIO::GetRegSize)
        .def("WriteAddress",           &IPBusIO::WriteAddress)
        .def("WriteRegister",          &IPBusIO::WriteRegister);

    py::class_<ApolloSM, IPBusConnection, IPBusIO>(m, "ApolloSM")
        .def(py::init<std::vector<std::string> const &>())
        .def("GenerateStatusDisplay",  &ApolloSM::GenerateStatusDisplay)
        .def("GenerateHTMLStatus",     &ApolloSM::GenerateHTMLStatus)
        .def("GenerateGraphiteStatus", &ApolloSM::GenerateGraphiteStatus)
        .def("UART_Terminal",          &ApolloSM::UART_Terminal)
        .def("UART_CMD",               &ApolloSM::UART_CMD,      py::arg("promptChar") = '%')
        .def("svfplayer",              &ApolloSM::svfplayer,     py::arg("displayProgress") = false)
        .def("PowerUpCM",              &ApolloSM::PowerUpCM,     py::arg("wait") = -1)
        .def("PowerDownCM",            &ApolloSM::PowerDownCM,   py::arg("wait") = -1)
        .def("unblockAXI",             &ApolloSM::unblockAXI,    py::arg("name") = "")
        .def("DebugDump",              &ApolloSM::DebugDump)
        .def("restartCMuC",            &ApolloSM::restartCMuC)
        .def("GetSerialNumber",        &ApolloSM::GetSerialNumber)
        .def("GetRevNumber",           &ApolloSM::GetRevNumber)
        .def("GetShelfID",             &ApolloSM::GetShelfID)
        .def("GetSlot",                &ApolloSM::GetSlot)
        .def("GetZynqIP",              &ApolloSM::GetZynqIP)
        .def("GetIPMCIP",              &ApolloSM::GetIPMCIP);
}