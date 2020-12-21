SECTION rodata_user

PUBLIC _err_no_memory
PUBLIC _err_missing_filename
_err_no_memory:
  defm "Out of memor", 'y' + 0x80
;      "+++Longest valid erro", 'r' + 0x80
_err_missing_filename:
  defm "F Missing file nam", 'e' + 0x80
;      "+++Longest valid erro", 'r' + 0x80