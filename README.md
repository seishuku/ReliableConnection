# ReliableConnection

Playing around with making some reliable UDP connection transfer functions, building on my already existing network code.<br><br>
Seems to be doing the thing it should, needs more hardness testing, but it's just a simple CRC32, so it's not super robust (nor is it supposed to be).<br><br>
It will retry 3 times if CRC is mismatched, input buffer must be sized with extra space of a UINT32 for the CRC, I don't really like this, but I didn't want to allocate and copy data.