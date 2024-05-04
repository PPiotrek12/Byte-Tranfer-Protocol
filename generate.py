import os

files_list: dict[str, int] = {
  "empty": 0,
  "tiny": 64,
  "small": 1024,
  "two_packets": 65712,
  "three_packets": 199742,
  "medium": 631029,
  "large": 6746532
}

def get_file_name(name: str) -> str:
  return name + ".bin"

def generate(file_name: str, size: int):
  with open(file_name, "wb") as file:
    random_bytes = os.urandom(size)
    file.write(random_bytes)

def main():
  # Load a list of files in this directory
  file_in_dir: list[str] = os.listdir()
  print(f"Files in directory: {file_in_dir}")

  for name, size in files_list.items():
    file_name: str = get_file_name(name)

    if file_name in file_in_dir:
      print(f"File {file_name} already exists.")
    else:
      generate(file_name, size)
      print(f"File {file_name} generated.")

if __name__ == "__main__":
  main()
