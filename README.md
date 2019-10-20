# ByePg: Defeating Patchguard using Exception-hooking

ByePg hijacks the HalPrivateDispatchTable table to create a early-bugcheck hook. Utilizing this early-bugcheck hook it collects information about the exception and basically provides a simple interface to register a high-level system-wide exception handler.

A variety of kernel hooks can be implemented using this method completely bypassing PatchGuard and HVCI as it creates an entirely new attack surface, exception-based hooking, which was previously not possible in Windows kernel.

## Writeup:
-----
https://blog.can.ac/2019/10/19/byepg-defeating-patchguard-using-exception-hooking/

## Samples:
-----
- `\ByePgLib` contains the base library
- `\ExceptionHookingDemo` demonstrates the exception handler 
- `\InfinityHookFix` contains a sample rendering the recent InfinityHook patch by Microsoft useless