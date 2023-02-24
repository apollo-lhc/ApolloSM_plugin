#
# Python script to test Python binding of ApolloSM class.
#

# Should be using Python >= 3.6
import sys
if sys.version_info < (3,6):
    raise Exception("Please use Python 3.6 or a more recent version.")

from ApolloSM import ApolloSM

sm = ApolloSM(["/opt/address_table/connections.xml"])

# Test a read
register = "PL_MEM.USERS_INFO.USERS.COUNT"
value = sm.ReadRegister(register)
print("Test read:")
print(f"{register}: {value}")

# Test a write
scratch_register = "PL_MEM.SCRATCH.WORD_00"
sm.WriteRegister(scratch_register, 1)
print(f"Write 1 to {scratch_register}")
print(f"Reading back: {sm.ReadRegister(scratch_register)}")

# Write 0 back
sm.WriteRegister(scratch_register, 0)
print(f"Write 0 to {scratch_register}")
print(f"Reading back: {sm.ReadRegister(scratch_register)}")
