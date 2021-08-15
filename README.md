# zzdicom -- quick and dirty DICOM tools

The zzdicom package consists of a few fast and lightweight tools for reading, writing, verifying
and anonymizing DICOM data. They were written for practical use when and where I had need of them,
for easier debugging of DICOM issues, and not for correctness or completeness.

## Limitations

zzdicom currently only works with little-endian DICOM data. I have never encountered big-endian 
DICOM data in the real world (ie hospital settings), so I never saw a need for adding support for 
this. It should compile out of the box on Linux and MacOSX. I never tried to compile it on
Microsoft Windows, and I expect that this would require some modifications.

## Dependencies

The core parser has no dependencies than the system C library. The DICOM database functions require
SQLite. The texture code requires CharLS. The OpenGL utility code tests require GLUT. The sample
DICOM viewer program requires Qt.

## Features

The software was written with performance as a high priority, and the little it does, it does far
faster than any other DICOM software that I have tested. Memory management is very simple and
easy to validate. Unit test coverage of the core code is very high (over 90%). The parser is very 
robust, and when it encounters problems it shall always fail gently.

## Installation

To build:

    mkdir -p build
    cd build
    cmake ..
    make -j

To run the tests, add:

    make test

Set the TEST_GL environment variable before running cmake if you want to run the OpenGL tests.

## The tools

* zzanon - an in-place DICOM anonymizer. It *changes* the files you list, overwriting the tags it
recognizes as containing patient identifying information. It will not change *anything else*, 
including the relative positioning of any tags in the file. This is to retain the file exactly
as it was, including any esoteric errors the file might contain. Too many other anonymizer tools
will parse the file before copying it out again, accidentially fixing several classes of errors
on the way.
* zzdump - will dump a list of tags for each DICOM file given in a format very similar to DCMTK's
dcmdump. It also runs a number of validation checks on each tag, and prints errors found line 
by line.
* zzgroupfix - will run fix erroneous group size tags, useful as some software relies on these being
correct if they are present.
* zzprune - checks and updates all entries in the local DICOM database, removing entries that no
longer refer to an existing file.
* zzmkrandom - generates a pseudo-random DICOM file for use in manual testing or a unit testing 
framework. You can pass a random seed on the command line, or let it generate its own random
seed. The random seed used is saved in the Instance Number tag.
* zzpixel - manipulates pixel values in a DICOM file. Note that this also makes changes in-place.
* zzcopy - copy and convert DICOM files. Can transform from grayscale to RGB, or compress with
the JPEG-LS lossless algorithm.

In the nifti directory, you can also find these very experimental tools:

* zzdcm2nifti - convert file from a multi-frame DICOM to nifti format.
* zznifti2dcm - convert file from nifti to DICOM format, losing a lot of data in the process.
* zzniftistitch - convert a file from nifti to DICOM format, retrieving positioning and patient
information from the original DICOM series.

## Credits

This project would not have been possible without reusing some content from the DCMTK and GDCM
toolkits. Headers were adapted from DCMTK, and data definitions XML files were copied from GDCM.
Inspiration and knowledge were drawn from both, as well as from the dicom3tools toolkit. 

## Copyright

The software and data included are, unless otherwise specified, copyright (C) Per Inge Mathisen.

### Copyright - software

Permission is hereby granted, free of charge, to any person or organization obtaining a copy of
the software and accompanying documentation covered by this license to use, reproduce, display,
distribute, execute, and transmit the Software, and to prepare derivative works of the software, 
and to permit third-parties to whom the Software is furnished to do so.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE
AND NON-INFRINGEMENT. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE 
SOFTWARE BE LIABLE FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

### Copyright - data

* spine.dcm -- (C) Per Inge Mathisen, license CC BY-SA 3.0 (http://creativecommons.org/licenses/by-sa/3.0/)
* broccoli.dcm, corn.dcm, cucumber.dcm -- (C) Andy Ellison, license CC BY-SA 3.0 (http://creativecommons.org/licenses/by-sa/3.0/)
* minimal.hdr, minimal.img, minimal.nii -- public domain, copied from the nifti home page
* SIEMENS_GBS_III-16-ACR_NEMA_1-ULis2Bytes.dcm, SIEMENS_CSA2.dcm -- from gdcm test data package
