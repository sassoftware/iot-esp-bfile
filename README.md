# Blob file Connector for SAS Event Stream Processing

## Table of Contents

* [Overview](#overview)
	* [What it Does](#what-it-does)
	* [How it Works](#how-it-works)
		* [Publisher](#publisher)
		* [Subscriber](#subscriber)
		* [Connector properties](#connector-properties)
			* [Publisher](#publisher)
			* [Subscriber](#subscriber)
* [Prerequisites](#prerequisites)
	* [Hardware Requirements](#hardware-requirements)
	* [Software Requirements](#software-requirements)
* [Setup](#setup)
	* [Linux](#linux)
* [Running](#running)
* [Contributing](#contributing)
* [License](#license)
* [Additional Resources](#additional-resources)


## Overview


| Details            |                 |
|-----------------------|------------------|
| Target OS:            |  Linux (64-bit) or Windows see [SAS Supported Distributions](https://support.sas.com/en/documentation/third-party-software-reference/viya/34/support-for-operating-systems.html#windows)     |
| Programming Language  |  C++ |
| Time to complete:     |  10 min      |

### What it Does

  This publisher connector for SAS Event Stream Processing allows reading multiple files from a directory and publishing the whole file's content as birary blobs or single strings to a SAS Event Stream Processing model. 
  Used as an ESP subscriber connector, it allows writing each event blob field or string field as a single file. 
  This is very useful for reading a set of images and inject them as events to ESP, or to write image frames coming from each ESP events as a set of jpeg files. 

### How it Works

#### Publisher 
This connector parse a directory for files with name matching a specific regex pattern, and publishes those files to ESP. Each file contentis read and published to an ESP source window as an event. THe ESP source window schema must have 2 or 3 fields of type **`int64*,blob`** or **`int64*,string`** or **`int64*,rstring`** or **`int64*,blob,string`** or **`int64*,string,string`** or **`int64*,rstring,string`**". If the second field is type string, the file will be read as string. If it is type blob, the file will be read as binary.If the 3rd field is present, it will contain the filename. 

 or  The field names can be different, but the type and order of the field must be respected. The image will be published in the event blob field in JPEG or SAS wide format (uncompressed). Depending on the OpenCV Video I/O backend, it can read streams from video files, RTSP streams, video cameras, and many other OpenCV supported input streams. Refer to the [OpenCV](https://opencv.org) documentation for more details.

#### Subscriber
This subscriber connectors writes each event's field specified by **`datafieldname`** parameter as a file in the directory specified by parameter **`filename`**. Each file name is automatically appended with an monotoneously increasing integer. 

#### Connector properties

##### Publisher 
| property | values | default | description |
|----------|--------|--------|-------------|
| type | pub | - | This is an ESP publisher|
| path | *string*|-| The directory path that contains the files to read|
| filename_rgx |*string*|-|The regex expression for the file names to read|
| publishrate |*integer*|0| Specifies the publish rate (frames per second). Use `0` for using the maximum speed|
| repeatcount |*integer*|0| Number of times to repeat the file reading|

##### Subscriber 
| property | values | default | description |
|----------|--------|--------|-------------|
| type | pub | - | This is an ESP publisher|
| snapshot | true/false | true | Whether to write the snapshot|
| filename |*string*|-|The path and filename for the files to write|
| datafieldname | *string* | -| The ESP field name that contains the data to wrie to a file)|

## Prerequisites

### Hardware Requirements

See SAS Event Stream Processing  hardware requirements in the [ESP Deployment Guide](http://support.sas.com/documentation/onlinedoc/esp/index.html).

### Software Requirements

* SAS Event Stream Processing supported Operating System (Linux or windows)
* SAS Event Stream Processing 6.2 or above

* g++ compiler
  
It has been successfully tested with:

* CentOS 7.5
* SAS Event Stream Processing 6.2 and 2021.x

## Setup

### Linux

1. Install SAS Event Stream Processing and ensure it is properly installed.
2. Retrieve the connector code

    ```sh
    git clone git@gitlab.sas.com:IOT/tools/esp-bfile.git
    ```

5. Compile the connector plugin

   ```sh
   cd esp-bfile
   mkdir plugins
   make
   ```

6. The ESP connector plugin should be built in the `esp-bfile/plugins` directory
7. Copy the plugin in the ESP plugins folder

   ``` sh
   cp plugins/x86_64/libesp_bfile_cpi.so $DFESP_HOME/lib/plugins/
   ```

8. Restart SAS Event Stream Processing if it was already running.


## Running

The provided sample read and write images into an ESP model.

```sh
dfesp_xml_server -http 61000 -pubsub 61001 -model file://sample/bfile_sample.xml
```


## Contributing

> We welcome your contributions! Please read [CONTRIBUTING.md](CONTRIBUTING.md) for details on how to submit contributions to this project. 

## License

> This project is licensed under the [Apache 2.0 License](LICENSE).

## Additional Resources

- [SAS Event Stream Processing 2021.1.4 Documentation](https://go.documentation.sas.com/doc/en/espcdc/v_013/espwlcm/home.htm)

