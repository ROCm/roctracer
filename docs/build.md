## Build and run tests

- ROCm is required

- Packages required:

  1. For Ubuntu 18.04 and Ubuntu 20.04 the following adds the needed packages:

   ````shell
   apt install python3 python3-pip gcc g++ libatomic1 make \
    cmake doxygen graphviz texlive-full
   ````

  2. For CentOS 8.1 and RHEL 8.1 the following adds the needed packages:

   ````shell
   yum install -y python3 python3-pip gcc gcc-g++ make \
    cmake libatomic doxygen graphviz texlive \
    texlive-xtab texlive-multirow texlive-sectsty \
    texlive-tocloft texlive-tabu texlive-adjustbox
   ````

  3. For SLES 15 Service Pack 15 the following adds the needed packages:

   ````shell
   zypper in python3 python3-pip gcc gcc-g++ make \
    cmake libatomic doxygen graphviz \
    texlive-scheme-medium texlive-hanging texlive-stackengine \
    texlive-tocloft texlive-etoc texlive-tabu
   ````

- Python modules requirements: `CppHeaderParser`, `argparse`.

  To install:

  ```sh
  pip3 install CppHeaderParser argparse
  ```

- Clone development branch of `roctracer`:

  ```sh
  git clone -b amd-master https://github.com/ROCm-Developer-Tools/roctracer
  ```

- To build `roctracer` library:

   ```sh
   cd <your path>/roctracer
   ./build.sh
   ```

- To build and run test:

  ```sh
  cd <your path>/roctracer/build
  make mytest
  run.sh
  ```
