=================================================================================
== ApolloSM class & device plugin for BUTool
=================================================================================
This contains a hw class for the ApolloSM uHAL interface and a device plugin 
for BUTool that wraps the hw class.

=================================================================================
== Basic Build instructions
=================================================================================
To build this code, you will need Xilinx Petalinux 2018.2 ,UIO uHAL, 
  BUTool, and the BUTool IPBUS helpers. 

Getting and building uHAL UIO (requires petalinux)
Clone https://github.com/apollo-lhc/ipbus-software.git (cross-compilation branch)
Set the following enviornment variable
  export IPBUS_PATH=/PATH/TO/IPBUS
  export CACTUS_ROOT=/PATH/TO/IPBUS
Build with make.sh
Modify and run coplibs.sh to copy the uHAL libraries to the zynq

Now check-out BUTool (http://gauss.bu.edu/svn/butool/trunk)
  Then put this (ApolloSM) code and butool-ipbus-helpers in the plugins directory.
  check out http://gauss.bu.edu/svn/butool-ipbus-helpers/trunk as butool-ipbus-helpers

Then type "make"
Then update and run ./make/zynqCopy.sh to copy all the needed libraries to the zynq

=================================================================================
== Basic run instructions
=================================================================================
TODO: Example connections.xml and apollo_table.xml

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
