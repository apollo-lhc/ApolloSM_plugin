=================================================================================
== ApolloSM class & device plugin for BUTool
=================================================================================
This contains a hw class for the ApolloSM uHAL interface and a device plugin 
for BUTool that wraps the hw class.

=================================================================================
== Basic Build instructions
=================================================================================
To build this code, you will need uhal, UIO uHAL,BUTool, and the BUTool IPBUS helpers. 

It is easiest to build this repo when it is in the plugins dir of BUTool.

If you are re-building this on a machine with BUTool already installed (aka an Apollo blade),
you can use the following command to figure out the env variables you need to build it alone.

IPBUS_REG_HELPER_PATH=/opt/BUTool/ BUTOOL_PATH=/opt/BUTool/ CACTUS_ROOT=/opt/cactus/ UIO_UHAL_PATH=/opt/UIOuHAL/ make
INSTALL_PATH=/opt/BUTool/ IPBUS_REG_HELPER_PATH=/opt/BUTool/ BUTOOL_PATH=/opt/BUTool/ CACTUS_ROOT=/opt/cactus/ UIO_UHAL_PATH=/opt/UIOuHAL/ make install



=================================================================================
== Basic run instructions
=================================================================================
BUTool -a connections.xml

=================================================================================
== Code structure
=================================================================================
include/ApolloSM/ApolloSM.hh:

This is the class for the Apollo SM board.
This class is to be included in other code (SC/DAQ) to talk to the hardware.
This code inherits some useful IPBus IO wrappers from the IPBusIO class.

include/ApolloSM_device/ApolloSM_device.hh:

This class is a device plugin for the BUTool to give the hw class above a CLI 
interface.
It does this by having wrapper functions for the functions in the hw class that 
translate command names and arguments into calls of hw functions.

New commands are added by adding a member function with the following signature
CommandReturn::status ExampleCommand(std::vector<std::string>,std::vector<uint64_t>);
This command is handed an array of space separated strings and the blind uint64_t 
conversions of these strings. 
Once this member function exists, it is added as a command in the LoadCommandList()
function.
This function calls the helper function AddCommand() that connects a command name
string with a member function to be called.
It also allows for a command help and a callback function for auto-completing arguments. 
