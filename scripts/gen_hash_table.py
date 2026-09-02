from secrets import randbits

if __name__ == "__main__":
  entries = [f"0x{randbits(64):016x}ull" for _ in range(256)]
  out = "const static u64 cdc_hash_table[256] = {\n"
  for i in range(0, 256, 4):
    out += "\t" + ", ".join(entries[i:i+4]) + ",\n"
  out += "};"
  with open("cdc_hash_table.ex.c", "w") as f:
    f.write(out)