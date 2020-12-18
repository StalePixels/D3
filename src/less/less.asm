SECTION rodata_user

PUBLIC _err_no_memory
_err_no_memory:
  defm "Out of memor", 'y' + 0x80
;      "+++Longest valid erro", 'r' + 0x80