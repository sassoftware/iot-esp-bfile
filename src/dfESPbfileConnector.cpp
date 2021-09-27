// Copyright © 2021, SAS Institute Inc., Cary, NC, USA.  All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0


// -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

// api includes 
//
#include "dfESPbfileConnector.h"
#include "Base64.h"
#include "int/dfESPconvUtils.h"
#include <boost/lexical_cast.hpp>
#include <chrono>

#include "portDir.h"
#include "portFileIO.h"

#include <regex>

#include "boost/algorithm/string/trim.hpp"


using namespace std;

#if DEBUG_PUBSUBCLIENT
static int64_t eventsInjected = 0;
static int64_t eventsReceived = 0;
#endif

// NOTE: For this connector, the general rule of thumb is to disconnect from the queue manager in case of error, by calling freeResources() or stop().

dfESPconnectorInfo connectorInfo = {
    type_BOTH,
    dfESPbfileConnector::initialize,
    dfESPbfileConnector::subRequiredConfig,
    dfESPbfileConnector::sizeofSubReqConfig,
    dfESPbfileConnector::pubRequiredConfig,
    dfESPbfileConnector::sizeofPubReqConfig,
    dfESPbfileConnector::subOptionalConfig,
    dfESPbfileConnector::sizeofSubOptConfig,
    dfESPbfileConnector::pubOptionalConfig,
    dfESPbfileConnector::sizeofPubOptConfig,
};

dfESPconnectorInfo *getConnectorInfo() {
    return &connectorInfo;
}

dfESPstring dfESPbfileConnector::bfileSubAnnotationsFormatValues[] = {"astore", "tracking"};
dfESPstring dfESPbfileConnector::bfileSubAnnotationsCoordTypeValues[] = {"rect", "yolo", "coco"};
// dfESPstring dfESPbfileConnector::bfileSubFileTypeValues[] = {"jpg", "tif", "bmp"};

//
// Publisher config
// 
dfESPconnectorParmInfo_t dfESPbfileConnector::pubRequiredConfig[] = {
    {"type", "pub", dfESPconnector::sizeofPubSubValues, dfESPconnector::pubsubValues, false},
    {"project", "", 0, NULL, true},
    {"continuousquery", "", 0, NULL, true},
    {"window", "", 0, NULL, true},

    {"path", "", 0, NULL, false},
    {"filename_rgx", "", 0, NULL, false}
};
size_t dfESPbfileConnector::sizeofPubReqConfig =
    sizeof(dfESPbfileConnector::pubRequiredConfig)/sizeof(dfESPconnectorParmInfo_t);

dfESPconnectorParmInfo_t dfESPbfileConnector::pubOptionalConfig[] = {
    {"host", "", 0, NULL, true},
    {"port", "", 0, NULL, true},
    {"token", "", 0, NULL, true},
    {"tokenlocation", "", 0, NULL, true},
    
    {"publishrate", "0", 0, NULL, false},
    {"repeatcount", "0", 0, NULL, false},
    
    {"transactional", "false", dfESPconnector::sizeofTrueFalseValues, dfESPconnector::trueFalseValues, false},
    {"blocksize", "1", 0, NULL, false},
    {"configfilesection", "", 0, NULL, false},
    {"publishwithupsert", "false", dfESPconnector::sizeofTrueFalseValues, dfESPconnector::trueFalseValues, false},
    
};
size_t dfESPbfileConnector::sizeofPubOptConfig =
    sizeof(dfESPbfileConnector::pubOptionalConfig)/sizeof(dfESPconnectorParmInfo_t);

// Subscriber config
//     
dfESPconnectorParmInfo_t dfESPbfileConnector::subRequiredConfig[] = {
    {"type", "sub", dfESPconnector::sizeofPubSubValues, dfESPconnector::pubsubValues, false},
    {"project", "", 0, NULL, true},
    {"continuousquery", "", 0, NULL, true},
    {"window", "", 0, NULL, true},
    {"snapshot", "true", dfESPconnector::sizeofTrueFalseValues, dfESPconnector::trueFalseValues, false},
    
    {"filename", "", 0, NULL, false},
    {"datafieldname", "", 0, NULL, false}
};
size_t dfESPbfileConnector::sizeofSubReqConfig =
    sizeof(dfESPbfileConnector::subRequiredConfig)/sizeof(dfESPconnectorParmInfo_t);

dfESPconnectorParmInfo_t dfESPbfileConnector::subOptionalConfig[] = {
    {"host", "", 0, NULL, true},
    {"port", "", 0, NULL, true},
    {"token", "", 0, NULL, true},
    {"tokenlocation", "", 0, NULL, true},

    /*{"filetype", "jpg", sizeof(bfileSubFileTypeValues)/sizeof(dfESPstring), bfileSubFileTypeValues, false}, */

    {"collapse", "false", dfESPconnector::sizeofTrueFalseValues, dfESPconnector::trueFalseValues, false},
    {"rmretdel", "false", dfESPconnector::sizeofTrueFalseValues, dfESPconnector::trueFalseValues, false},
    {"dateformat", "", 0, NULL, false},

    {"configfilesection", "", 0, NULL, false},

};
size_t dfESPbfileConnector::sizeofSubOptConfig =
    sizeof(dfESPbfileConnector::subOptionalConfig)/sizeof(dfESPconnectorParmInfo_t);


// NOTE: for adapter error reporting, make sure to set _errorKey, _errorValue, and _errorReason when logging any configuration related error
bool dfESPbfileConnector::start() {


    if (_started || _client) {
        eLOG_ERROR("Connectors0001", (  "dfESPbfileConnector::start()", "already" ) );
        if (_errorCallback) { _errorCallback(ESP_PUBSUBFAIL_CONNECTORFAIL, ESP_PUBSUBCODE_NOERROR, _ctx); }
        return false;
    }
    //
    // Check pub/sub type
    //
    if (_type == type_SUB) {
        if (!checkConfig(subRequiredConfig, sizeofSubReqConfig, subOptionalConfig,sizeofSubOptConfig)) {
            return false;
        }
    } else if (_type == type_PUB) {
        if (!checkConfig(pubRequiredConfig, sizeofPubReqConfig, pubOptionalConfig,sizeofPubOptConfig)) {
            return false;
        }
    } else {
        _errorKey = "type";
        _errorValue = "";
        _errorReason = PARM_MISSING;
        eLOG_ERROR("Connectors0005", (  "dfESPbfileConnector::start()", "type" ) );
        if (_errorCallback) {_errorCallback(ESP_PUBSUBFAIL_CONNECTORFAIL,ESP_PUBSUBCODE_NOERROR, _ctx);}
        return false;
    }

    _transactional = (getParameter("transactional") == "true");

   
    if (_type == type_SUB) {
        //
        // check SUBSCRIBER parameters
        //
        _outputFileName = getParameter("filename");

        if ((_outputFileName == "") ) {
            _errorKey = "filename";
            _errorValue = "";
            _errorReason = PARM_MISSING;
            eLOG_ERROR("Connectors0005", (  "dfESPbfileConnector::start()","filename" ) );
            if (_errorCallback) {_errorCallback(ESP_PUBSUBFAIL_CONNECTORFAIL,ESP_PUBSUBCODE_NOERROR, _ctx); }
            return false;
        }

        _outputFilePath = _outputFileName.substr(0,_outputFileName.find_last_of('.')) ;
        _outputFileExtension = _outputFileName.substr(_outputFileName.find_last_of('.') ); 


        if (!startSub()) {
            return false;
        }
    } else if (_type == type_PUB) {
        //
        // check PUBLISHER parameters
        //

        //
        // path
        //
        _filePath = getParameter("path");

        if ((_filePath == "") ) {
            _errorKey = "path";
            _errorValue = "";
            _errorReason = PARM_MISSING;
            eLOG_ERROR("Connectors0005", (  "dfESPbfileConnector::start()","path" ) );
            if (_errorCallback) {_errorCallback(ESP_PUBSUBFAIL_CONNECTORFAIL,ESP_PUBSUBCODE_NOERROR, _ctx); }
            return false;
        }
        //
        // filename_rgx
        //
        _fileNameRgx = getParameter("filename_rgx");

        if ((_fileNameRgx == "") ) {
            _errorKey = "filename_rgx";
            _errorValue = "";
            _errorReason = PARM_MISSING;
            eLOG_ERROR("Connectors0005", (  "dfESPbfileConnector::start()", "filename_rgx" ) );
            if (_errorCallback) {_errorCallback(ESP_PUBSUBFAIL_CONNECTORFAIL,ESP_PUBSUBCODE_NOERROR, _ctx); }
            return false;
        }
        //
        // publishrate
        //
         dfESPstring value = getParameter("publishrate");
        _publishRate=stod(string(value.c_str()));
        //
        // repeatcount
        //
        dfESPstring repeatCount = getParameter("repeatcount");
        if (!dfESPconvUtils::ato32(repeatCount.c_str(), &_repeatCount) || _repeatCount < 0) {
            _errorKey = "repeatcount";
            _errorValue = repeatCount.c_str();
            _errorReason = INVALID_VALUE;
            eLOG_ERROR("Connectors0008", ( "dfESPbfileConnector::start()", "repeatcount", repeatCount ) );
            if (_errorCallback) { _errorCallback(ESP_PUBSUBFAIL_CONNECTORFAIL,ESP_PUBSUBCODE_NOERROR, _ctx); }
            return false;
        }
  
        if (!startPub()) {
            return false;
        }
    } else {
        return false;
    }

    dfESPconnector::setState(state_RUNNING);
    return true;
}


bool dfESPbfileConnector::setupCallbackFunction(dfESPschema *schema, bool winIsAutogen) {
    // schema related setup
    _schema = schema;
    _winIsAutogen = winIsAutogen;
    dfESPstring *names = _schema->getNames();
    dfESPdatavar::dfESPdatatype *types = _schema->getTypes();
    
    bool schemaOk = true;
    if (_type == type_PUB) {
        //
        // check source window schema
        //
        if (_schema->getNumFields() >= 2) {

            if ( _schema->getTypeEO(0) != dfESPdatavar::ESP_INT64    || 
                ( _schema->getTypeEO(1) != dfESPdatavar::ESP_BINARY  &&
                  _schema->getTypeEO(1) != dfESPdatavar::ESP_UTF8STR &&
                  _schema->getTypeEO(1) != dfESPdatavar::ESP_RUTF8STR) )  {

                schemaOk = false;  
            }
        } 
        if (_schema->getNumFields() == 3) {
            if (_schema->getTypeEO(2) != dfESPdatavar::ESP_UTF8STR) {
                schemaOk = false;  
            }   
        } 
        if (_schema->getNumFields() > 3) {
            schemaOk = false;  
        }

        if (schemaOk == false ){
            eLOG_ERROR("Connectors0110", 
                      ( "Source window schema must have 2 or 3 fields of type int64/blob or int64/string or int64/rstring or int64/blob/string or int64/string/string or int64/rstring/string" ) );
            if (_errorCallback) { _errorCallback(ESP_PUBSUBFAIL_CONNECTORFAIL, ESP_PUBSUBCODE_NOERROR, _ctx); }
            return false;
        }

        if (_schema->getTypeEO(1) == dfESPdatavar::ESP_BINARY) {
            _publishAsBinary = true;
        }


    } else if (_type == type_SUB) { 
        //
        // check SUBSCRIBER parameters
        //
        dfESPstring dataFieldName = getParameter("datafieldname");
        if (!dataFieldName.empty()) {
            _dataFieldIdIO = _schema->findIndexIO(dataFieldName);
        }
        if (_dataFieldIdIO == -1) {
            eLOG_ERROR("Connectors0076", ( "dfESPbfileConnector::setupCallbackFunction()", "datafieldname" ) );
            if (_errorCallback) { _errorCallback(ESP_PUBSUBFAIL_CONNECTORFAIL, ESP_PUBSUBCODE_NOERROR, _ctx); }
            return false;
        }

        if (_schema->getTypeIO(_dataFieldIdIO) == dfESPdatavar::ESP_BINARY) {
            _publishAsBinary = true;
        } else if (_schema->getTypeIO(_dataFieldIdIO) != dfESPdatavar::ESP_UTF8STR &&
                   _schema->getTypeIO(_dataFieldIdIO) != dfESPdatavar::ESP_RUTF8STR ) {

            _errorKey = "datafieldname";
            _errorValue = dataFieldName;
            _errorReason = INVALID_VALUE;
            eLOG_ERROR("Connectors0008", ( "dfESPbfileConnector::setupCallbackFunction()","datafieldname", dataFieldName ) );
            if (_errorCallback) { _errorCallback(ESP_PUBSUBFAIL_CONNECTORFAIL, ESP_PUBSUBCODE_NOERROR, _ctx) ;}
            return false;
        }
        

    } 

    return true;
}

void dfESPbfileConnector::errorCallbackFunction(dfESPpubsubFailures_t failure,
                                               dfESPpubsubFailureCodes_t code) {
    eLOG_ERROR("Connectors0019", ( 
               "dfESPbfileConnector::errorCallbackFunction()",
               C_dfESPdecodePubSubFailure((C_dfESPpubsubFailures)failure),
               C_dfESPdecodePubSubFailureCode((C_dfESPpubsubFailureCodes)code) ) );
    if (_errorCallback) {
        // call application callback
        _errorCallback(failure, code, _ctx);
    }
}

dfESPbfileConnector::dfESPbfileConnector(dfESPengine *engine, dfESPpsLib_t psLib,
                                   dfESPstring name, dfESPstring xportCfgFile) {
    // init(psLib, engine, name, xportCfgFile);
    if (!init(psLib, engine, name, xportCfgFile)) {
        return;
    }

}

dfESPconnector *dfESPbfileConnector::initialize(dfESPengine *engine,
                                             dfESPpsLib_t psLib,
                                             dfESPstring name,
                                             dfESPstring xportCfgFile) {
    // return new dfESPbfileConnector(engine, psLib, name, xportCfgFile);
    dfESPconnector *conn = new dfESPbfileConnector(engine, psLib, name, xportCfgFile);
    if (conn->_initError) {
        conn = NULL;
    }
    return conn;

}

dfESPbfileConnector::~dfESPbfileConnector() {
    stop();
    if (_type == type_SUB) {
        // cleanup some sub stuff if required      
    }
}


void dfESPbfileConnector::freeResources() {
      
    if (_type == type_PUB) {
  
    }
}

bool dfESPbfileConnector::stop() {
    if (_pubThread) {
        _threadStop.inc();
        _pubThread->join();
        delete _pubThread;
        _pubThread = NULL;
        // checkCommit(0);
    }
    dfESPconnector::stop();
    _schema = NULL;
    freeResources();

#if DEBUG_PUBSUBCLIENT
    std::cout << "eventsInjected = " << eventsInjected << " eventsReceived = " << eventsReceived << std::endl;
#endif

    return true;
}

void bfilePubThread(void *ctx) {
    dfESPbfileConnector *bfileConnector = (dfESPbfileConnector *)ctx;
    bfileConnector->publisherThread();
}

bool dfESPbfileConnector::startPub() {

    dfESPstring blocksize = getParameter("blocksize");
    if (!dfESPconvUtils::ato32(blocksize.c_str(), &_blocksize)) {
        _errorKey = "blocksize";
        _errorValue = blocksize.c_str();
        _errorReason = INVALID_VALUE;
        eLOG_ERROR("Connectors0008", ( 
                   "dfESPbfileConnector::startPub()",
                   "blocksize", blocksize ) );
        if (_errorCallback) {
            // call application callback
            _errorCallback(ESP_PUBSUBFAIL_CONNECTORFAIL,
                           ESP_PUBSUBCODE_NOERROR, _ctx);
        }
        stop();
        return false;
    }

    // connect to ESP server
    if (!dfESPconnector::start()) {
        return false;
    }
    if (!_schema) {
        eLOG_ERROR("Connectors0005", ( 
                                      "dfESPbfileConnector::startPub()", "schema" ) );
        if (_errorCallback) {
            // call application callback
            _errorCallback(ESP_PUBSUBFAIL_CONNECTORFAIL,
                           ESP_PUBSUBCODE_NOERROR, _ctx);
        }
        stop();
        return false;
    }
    _pubThread = dfESPthreadUtils::thread::thread_create(bfilePubThread);
    if (!_pubThread) {
        eLOG_OALLOC_fault("dfESPthreadUtils::thread");
        if (_errorCallback) {
            // call application callback
            _errorCallback(ESP_PUBSUBFAIL_CONNECTORFAIL,
                           ESP_PUBSUBCODE_NOERROR, _ctx);
        }
        stop();
        return false;
    }
    _pubThread->setArgs((void *)this);
    int rc = _pubThread->start();
    if (rc < 0) {
        eLOG_ERROR("Connectors0007", ( "dfESPbfileConnector::startPub()", "bfilePubThread" ) );
        if (_errorCallback) {
            // call application callback
            _errorCallback(ESP_PUBSUBFAIL_CONNECTORFAIL,
                           ESP_PUBSUBCODE_NOERROR, _ctx);
        }
        stop();
        return false;
    }
    return true;
}


bool dfESPbfileConnector::getFileList() {
    //
    // Get the file list
    //
    port::Dir::PDIR_DIR *dir;
    std::string dot="."; 
    std::string dotdot=".."; 
    std::regex rgx; 
    try {
        rgx = _fileNameRgx.c_str();
    } catch (const std::regex_error& e) {
        ostringstream oss;
        oss << "Bad filename_rgx regex: " << _fileNameRgx.c_str() << " "<< e.what() ;
        
        if (e.code() == std::regex_constants::error_collate)    oss << " - The regex gives an error_collate"    ;
        if (e.code() == std::regex_constants::error_ctype)      oss << " - The regex gives an error_ctype"      ;
        if (e.code() == std::regex_constants::error_escape)     oss << " - The regex gives an error_escape"     ;
        if (e.code() == std::regex_constants::error_backref)    oss << " - The regex gives an error_backref"    ;
        if (e.code() == std::regex_constants::error_brack)      oss << " - The regex gives an error_brack"      ;
        if (e.code() == std::regex_constants::error_paren)      oss << " - The regex gives an error_paren"      ;
        if (e.code() == std::regex_constants::error_brace)      oss << " - The regex gives an error_brace"      ;
        if (e.code() == std::regex_constants::error_badbrace)   oss << " - The regex gives an error_badbrace"   ;
        if (e.code() == std::regex_constants::error_range)      oss << " - The regex gives an error_range"      ;
        if (e.code() == std::regex_constants::error_space)      oss << " - The regex gives an error_space"      ;
        if (e.code() == std::regex_constants::error_badrepeat)  oss << " - The regex gives an error_badrepeat"  ;
        if (e.code() == std::regex_constants::error_complexity) oss << " - The regex gives an error_complexity" ;
        if (e.code() == std::regex_constants::error_stack)      oss << " - The regex gives an error_stack"      ;
        eLOG_ERROR("Connectors0110", (  oss.str().c_str() ) ); 
        return false;
    }
    
    dir = port::Dir::opendir(_filePath.c_str());
    if (!dir) {
        ostringstream oss;
        oss << "ERROR: could not open directory: " << _filePath ;
        eLOG_ERROR("Connectors0110", (  oss.str().c_str() ) ); 
        return false;
    } else {
        port::Dir::PDIR_dirent *ent = port::Dir::readdir(dir);
        while (ent) {
            std::string fileName = port::Dir::get_name(ent);
            if ((fileName.compare(dot) != 0) && (fileName.compare(dotdot) != 0)) {
                if ( !port::Dir::is_dir(ent, _filePath.c_str()) && regex_match(fileName, rgx ) ) { 
                    fileName = _filePath.c_str() + std::string("/") + fileName; 
                    if (_processedFileList.find(fileName) == _processedFileList.end()) {
                        _workingFileList.push_back(fileName);
                    }
                    
                }
            }
            ent = port::Dir::readdir(dir);
        }
        port::Dir::closedir(dir);
    }
    //
    // sort file list
    //
    std::sort (_workingFileList.begin(),_workingFileList.end());
    // //debug
    // for (size_t i= 0 ; i< _workingFileList.size(); i++) {
    //     cout << "FILE: " << _workingFileList[i] << endl;
    // }

    return true;
}


void dfESPbfileConnector::publisherThread() {

    dfESPptrVect<dfESPeventPtr> trans;
    _schema->buildEventDatavarVect(_dvv);

    int64_t frameNumber = 1;

    bool error = false;

    if ( _publishRate <= 0.0) {
        _publishPeriod = 0;
    } else {
        _publishPeriod = static_cast<int32_t>(1000/_publishRate);
    }
        
    auto lastTime = std::chrono::system_clock::now();

    while(true) {
        //
        // Get the file list to read
        //
        if (!getFileList()) {
            eLOG_ERROR("Connectors0110", (  "Error getting file list" ) ); 
            error = true;
            break;
        }
        ostringstream oss;
        oss << "dfESPbfileConnector::publisherThread(): "<< "publishing " << _workingFileList.size() << " files as " << (_publishAsBinary? "binary":"string") << " fields" ;
        eLOG_INFO("Connectors0110", (  oss.str().c_str() ) ); 
        
        size_t i = 0;
        
        while ( i < _workingFileList.size() && 0 == _threadStop.get() ) {
            
            auto now = std::chrono::system_clock::now();

            if ( _publishPeriod == 0 || now - lastTime >= std::chrono::milliseconds(_publishPeriod)) {
                //
                // opening file
                //
                char * buff = nullptr;
                std::streampos fileSize;
                ios_base::openmode mode;

                if (_publishAsBinary) {
                    mode = ios::in|ios::binary|ios::ate;
                } else {
                    mode = ios::in|ios::ate;
                }
                std::ifstream file (_workingFileList[i].c_str(), mode);

                if (file.is_open())
                {
                    fileSize = file.tellg();
                    buff = (char*)malloc((int64_t)fileSize + 1); // adding 1 for adding the ending NULL in case of string. 
                    if (!buff) {
                        eLOG_MALLOC_fault((int64_t)fileSize);
                        error = true;
                        file.close();
                        break;
                    }
                    //
                    // reading file
                    //
                    file.seekg(0, ios::beg);
                    file.read(buff, fileSize);
                    file.close();

                    eLOG_DEBUG ("Connectors0032", (  "captured fileLength=", to_string(fileSize), "ok" ) );
                    //
                    // publishing file
                    //
                    
                    // ID
                    _dvv[0]->setValue(dfESPdatavar::ESP_INT64, &frameNumber);
                    // File content
                    if (_publishAsBinary) {
                        dfESPblob  *myBlob = dfESPblob::create(fileSize, buff, true);
                        _dvv[1]->setDataCopy(myBlob);
                        dfESPvblob::destroy(myBlob);
                    } else {
                        buff[fileSize] = NULL; // adding ending NULL to the string as it is probably not present in the file. 
                        _dvv[1]->setStringOrRstring(buff);
                    }
                    // File name
                    if (_dvv.size() == 3) {  
                        _dvv[2]->setStringOrRstring( (char*)_workingFileList[i].c_str() );
                    }

                    if(!buildEvent()) {
                        //failed to build event
                        error = true;
                        break;
                    }

                    free(buff);

                    _processedFileList.insert(_workingFileList[i]);
                }
                else  {
                    eLOG_ERROR("Connectors0110", (  "Unable to open file" ) ); 
                }

                frameNumber++;
                lastTime = now;
                ++i;
                
            }
            else {
                gMilliSleep( _publishPeriod / 10 );
            } 
            
        }

        if (_repeatCount < 0) { // endless loop until thread stop
            continue;
        }

        if (_repeatCount == 0 || error == true) {
            break;
        } else {
            _repeatCount--;
        }
       
    }

    dfESPconnector::setState(dfESPabsConnector::state_FINISHED);
    _started = false;
    return;

}

bool dfESPbfileConnector::startSub() {

    // connect to ESP server
    if (!dfESPconnector::start()) {
        return false;
    }
    return true;
}


#if 0

// trim from start (in place)
static inline void ltrim(std::string &s) {
    s.erase( s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {return !std::isspace(ch);} ) );
}

// trim from end (in place)
static inline void rtrim(std::string &s) {
    s.erase( std::find_if(s.rbegin(), s.rend(), [](int ch) {return !std::isspace(ch);} ).base(), s.end());
}
// trim from both ends (in place)
static inline void trim(std::string &s) {
    ltrim(s);
    rtrim(s);
}

// trim from start (copying)
static inline std::string ltrim_copy(std::string s) {
    ltrim(s);
    return s;
}

// trim from end (copying)
static inline std::string rtrim_copy(std::string s) {
    rtrim(s);
    return s;
}

// trim from both ends (copying)
static inline std::string trim_copy(std::string s) {
    trim(s);
    return s;
}
#endif

// NOTE: in case of error, do not call stop() inside callbackFunction(), as it tries to join
// on the calling thread...instead call freeResources()
bool dfESPbfileConnector::callbackFunction(dfESPeventblockPtr eventBlock, dfESPschema *schema) {
    bool rc = true;

    int32_t eventCnt = eventBlock->getSize();
    int32_t eventIndx;

    for (eventIndx=0; eventIndx < eventCnt; eventIndx++) {
        dfESPeventPtr event = eventBlock->getData(eventIndx);


        if (event->isNullIntID(_dataFieldIdIO)) {
            cout << "dfESPbfileConnector::callbackFunction() => NULL EVENT" << endl;
            continue;        
        }

        // FIXME is text or binary?


        std::vector<unsigned char> *buffV = nullptr;
        char* buff = nullptr;
        size_t buffSize = 0;
        
                    
        if (_publishAsBinary) { // data field is blob
            
            dfESPblob *imBlob = (dfESPblob *)event->getPtrByIntIndex(_dataFieldIdIO);
            buff = (char*)imBlob->getData();
            buffSize = imBlob->getLength();
        } else { // data field is string 
            // This gives a pointer to the string whether it is ESP_UTF8STR or ESP_RUTF8STR
            buff = event->getStringPtrByIntIndex(_dataFieldIdIO); 
            buffSize = strlen(buff);
        }
        
        string filePath = _outputFilePath.c_str() + to_string(static_cast<long long>(_frameNumber)) + _outputFileExtension.c_str(); 
#if 0
        ios_base::openmode mode = std::ofstream::out | std::ofstream::binary ;
        std::ofstream file (filePath.c_str(), mode);
        file.write(buff, buffSize);
        file.close();
#else        
        FILE* target;
        if (_publishAsBinary) {
            target = fopen(filePath.c_str(), "wb");
        } else {
            target = fopen(filePath.c_str(), "w");
        }
        
        if (target == nullptr) {
            ostringstream oss;
            oss << "Unable to create file : " << filePath.c_str() << endl;
            eLOG_ERROR("Connectors0110", (  oss.str().c_str() ) ); 
        } else {
            fwrite(buff, sizeof(char), (int)buffSize, target);
            //fwrite(buff, sizeof(unsigned char), (int)buffSize, target);
            fclose(target);
        }
        
#endif
        if (buffV) {
            delete buffV;
            buff = nullptr;
        } 

        _frameNumber++;
    }
   
    return rc;
}



bool dfESPbfileConnector::buildEvent() {
    dfESPeventPtr event = new dfESPevent();
    dfESPeventcodes::dfESPeventopcodes opcode = _publishwithupsert ?
        dfESPeventcodes::eo_UPSERT : dfESPeventcodes::eo_INSERT;
    if (!event->buildEvent(_schema, _dvv, opcode, dfESPeventcodes::ef_NORMAL)) {
        delete event;
        event = NULL;
        eLOG_ERROR("Connectors0003", ( "dfESPbfileConnector::buildEvent()",
                                       "dfESPevent" ) );
        if (_errorCallback) {
            // call application callback
            _errorCallback(ESP_PUBSUBFAIL_CONNECTORFAIL,
                           ESP_PUBSUBCODE_NOERROR, _ctx);
        }
        return false;
    }
    _trans.push_back(event);
    if (_trans.size() >= (size_t)_blocksize) {
        // build and inject event block
        dfESPeventblockPtr eventBlock =
            dfESPeventblock::newEventBlock(&_trans,
                                           (_transactional ? dfESPeventblock::ebt_TRANS :
                                            dfESPeventblock::ebt_NORMAL));
        _trans.free();
        if (!eventBlock) {
            eLOG_ERROR("Connectors0003", (
                       "dfESPbfileConnector::buildEvent()",
                       "dfESPeventblockPtr" ) );
            if (_errorCallback) {
                // call application callback
                _errorCallback(ESP_PUBSUBFAIL_CONNECTORFAIL,
                               ESP_PUBSUBCODE_NOERROR, _ctx);
            }
            return false;
        } else if (!pubInject(eventBlock)) {
            eLOG_ERROR("Connectors0015", ( "dfESPbfileConnector::buildEvent()" ) );
            if (_errorCallback) {
                // call application callback
                _errorCallback(ESP_PUBSUBFAIL_CONNECTORFAIL,
                               ESP_PUBSUBCODE_NOERROR, _ctx);
            }
            return false;
        }

#if DEBUG_PUBSUBCLIENT
        eventsInjected += eventBlock->getSize();
#endif

    }
    return true;
}

bool dfESPbfileConnector::isFailoverStandby() {
    return false;
}

