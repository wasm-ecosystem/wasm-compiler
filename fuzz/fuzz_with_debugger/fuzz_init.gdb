define start_fuzz
  set pagination off
  b main
  target remote localhost:$arg0
  continue
  source ./fuzz/fuzz_with_debugger/gdb_fuzz.py
end
