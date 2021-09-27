// Copyright © 2021, SAS Institute Inc., Cary, NC, USA.  All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0


// -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/**
 * \class dfESPbfileConnector
 *
 * \brief An blob file Connector object.
 *
 * @par Revision:
 * \$File$\n
 * \$Revision$ \$DateTime$ \$Change$\n
 * \$Author$ (last check-in)
 *
 * \ingroup dfESP_connectors
 *
 */

#ifndef __dfESPbfileConnector__
#define __dfESPbfileConnector__

// internal includes 
//
#include "int/dfESPexport.h"

// api includes 
//
#include "dfESPconnector.h"



class dfESPbfileConnector : public dfESPconnector {
    
    friend void bfilePubThread(void *ctx);
    
public:

    /**
     * static initializer used by connector manager
     */
    DFESPCONP_API static dfESPconnector *initialize(dfESPengine *engine,
                                                    dfESPpsLib_t psLib,
                                                    dfESPstring name,
                                                    dfESPstring xportCfgFile);

    //
    // Private member functions
    //
private:
    /**
     * dfESPbfileConnector constructor
     */
    DFESPCONP_API dfESPbfileConnector(dfESPengine *engine, dfESPpsLib_t psLib,
                                   dfESPstring name, dfESPstring xportCfgFile);
    /**
     * dfESPbfileConnector destructor
     */
    DFESPCONP_API ~dfESPbfileConnector();
    /**
     * Start the connector thread
     * @return bool true = success, false = failure
     */
    DFESPCONP_API bool start();
    /**
     * Stop the connector thread
     * @return bool true = success, false = failure
     */
    DFESPCONP_API bool stop();

    DFESPCONP_API bool isFailoverStandby();
    /**
     * The subscriber callback function
     * @param eventBlock event block pointer
     * @param schema pointer to the window schema
     */
    bool callbackFunction(dfESPeventblockPtr eventBlock, dfESPschema *schema);
    /**
     * The error callback function
     * @param failure the general failure
     * @param code the specific error code
     */
    void errorCallbackFunction(dfESPpubsubFailures_t failure,
                                       dfESPpubsubFailureCodes_t code);
    /**
     * The setup callback function, does any additional setup where schema ptr is
     * required, called by pubsubClient after schema is received & before returning
     * success code to server
     * @param schema pointer to the window schema
     * @param winIsAutogen whether the source window autogenerates the key field
     * @return true = success, false = failure
     */
    bool setupCallbackFunction(dfESPschema *schema, bool winIsAutogen);

    /**
     * Subscribe to an ESP project/query/window and publish received events to an mq topic
     * @return bool true = success, false = failure
     */
    bool startSub();
    /**
     * Subscribe to an mq topic and publish received events to an ESP project/query/window
     * @return bool true = success, false = failure
     */
    bool startPub();
    /**
     * The worker thread started by startPub()
     */
    void publisherThread();

    bool getFileList();

    bool buildEvent();

    void freeResources();

    void serializeDestroy(void *pSerialized);

public:
    static dfESPconnectorParmInfo_t subRequiredConfig[];
    static size_t sizeofSubReqConfig;
    static dfESPconnectorParmInfo_t pubRequiredConfig[];
    static size_t sizeofPubReqConfig;
    static dfESPconnectorParmInfo_t subOptionalConfig[];
    static size_t sizeofSubOptConfig;
    static dfESPconnectorParmInfo_t pubOptionalConfig[];
    static size_t sizeofPubOptConfig;

    //
    // Private state data
    //
private:
    static dfESPstring bfileSubAnnotationsFormatValues[];
    static dfESPstring bfileSubAnnotationsCoordTypeValues[];
    //static dfESPstring bfileSubFileTypeValues[];
    
    int32_t _blocksize;
    bool _transactional;
    dfESPptrVect<dfESPeventPtr> _trans;
    dfESPptrVect<dfESPdatavarPtr> _dvv;

    // Pub 
    dfESPstring _fileNameRgx;
    dfESPstring _filePath;
    std::vector<std::string> _workingFileList;
    std::set<std::string>  _processedFileList;

    bool _publishAsBinary = false;

    double  _publishRate    = 0.0; // frames per second -- if <= 0 then the max speed is used.
    int32_t _publishPeriod = 0;   // =1000/_publishRate ms
    int32_t _repeatCount   = 0;

    // Sub
    dfESPstring _outputFileName;
    dfESPstring _outputFilePath;
    dfESPstring _outputFileExtension;

    int64_t _frameNumber = 1;

    //dfESPstring _dataFieldName;
    int32_t _dataFieldIdIO = -1;
    int32_t _nbObjectsFieldIdIO = -1;
    
};

extern "C" {
    DFESPCONP_API dfESPconnectorInfo *getConnectorInfo();
}

#endif
