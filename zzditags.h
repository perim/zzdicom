// Experimental alternative network protocol for transfering DICOM data

#include "zz.h"

// keep as private group for now
#define DIGROUP 0x4096

// basic handshake
#define DCM_DiProtocolIdentification             ZZ_KEY(DIGROUP, 0x0002) // CS, always "DICOMDIRECT"
#define DCM_DiPeerName                           ZZ_KEY(DIGROUP, 0x0005) // LO, peer's AETITLE
#define DCM_DiPeerKeyHash                        ZZ_KEY(DIGROUP, 0x0007) // LO, hash of PGP key for optional validation
#define DCM_DiPeerCurrentDateTime                ZZ_KEY(DIGROUP, 0x0008) // DT
#define DCM_DiPeerStatus                         ZZ_KEY(DIGROUP, 0x0009) // CS, one of "ACCEPTED" (known) or "UNKNOWN" or "BAD DATETIME" or "FAILED" (pgp hash)
#define DCM_DiKeyChallenge                       ZZ_KEY(DIGROUP, 0x0021) // OB, random string encrypted with challenged's public key
#define DCM_DiKeyResponse                        ZZ_KEY(DIGROUP, 0x0022) // OB, string above, encrypted with challenger's public key
#define DCM_DiKeyStatus                          ZZ_KEY(DIGROUP, 0x0023) // CS, one of "ACCEPTED" or "FAILED"
//#define DCM_DiPriority                           ZZ_KEY(DIGROUP, 0x0024) // CS, optional, one of "HIGH", "NORMAL", "LOW", or "BULK"

// network service
#define DCM_DiNetworkServiceSequence             ZZ_KEY(DIGROUP, 0x1000) // SQ
#define DCM_DiNetworkService                     ZZ_KEY(DIGROUP, 0x1001) // CS, one of "STORE", "QUERY", "RETRIEVE", "INFO", "ACCESS"
#define DCM_DiNetworkServiceRequestor            ZZ_KEY(DIGROUP, 0x1002) // PN, person making request
#define DCM_DiNetworkServiceStatus               ZZ_KEY(DIGROUP, 0x1100) // CS, "ACCEPTED" or "SERVICE DENIED" or "NO SERVICE"

// storage
#define DCM_DiStorageNumberOfStudies             ZZ_KEY(DIGROUP, 0x2001) // US
#define DCM_DiStorageStudySequence               ZZ_KEY(DIGROUP, 0x2002) // SQ, contains one or more studies
#define DCM_DiStorageNumberOfSeries              ZZ_KEY(DIGROUP, 0x2003) // US
#define DCM_DiStorageSeriesSequence              ZZ_KEY(DIGROUP, 0x2004) // SQ, contains one or more series
#define DCM_DiStorageNumberOfObjects             ZZ_KEY(DIGROUP, 0x2005) // US
#define DCM_DiStorageObjectSequence              ZZ_KEY(DIGROUP, 0x2006) // SQ, contains one or more files (objects)
#define DCM_DiStorageMD5Hash                     ZZ_KEY(DIGROUP, 0x2007) // LO, MD5 hash of file
#define DCM_DiStorageMethod                      ZZ_KEY(DIGROUP, 0x2008) // CS, "NEW" or "UPDATE" or "REQUESTED" (if retrieve)
#define DCM_DiStorageSize                        ZZ_KEY(DIGROUP, 0x2090) // UX (new VR, unsigned 64bit), size of object to follow
#define DCM_DiStorageSequence                    ZZ_KEY(DIGROUP, 0x2091) // SQ, contains file to store
#define DCM_DiStorageResultSequence              ZZ_KEY(DIGROUP, 0x2101) // SQ, contains responses to storage items
#define DCM_DiStorageMethodStatus                ZZ_KEY(DIGROUP, 0x2102) // CS, "ACCEPTED", "DUPLICATE" (if new), "OLDER" (if update), 
                                                                         // or "REJECTED" (not allowed to update)
#define DCM_DiStorageResultStatus                ZZ_KEY(DIGROUP, 0x2103) // CS, "ACCEPTED", or "CHECKSUM ERROR", "DISK FULL" or generic "FAILED"

// query
#define DCM_DiQuerySequence                      ZZ_KEY(DIGROUP, 0x3001) // SQ, one item only, each tag in the item is AND'ed for query
#define DCM_DiQueryNumberOfResults               ZZ_KEY(DIGROUP, 0x3002) // UL
#define DCM_DiQueryResultSequence                ZZ_KEY(DIGROUP, 0x3003) // SQ, one item for each result
#define DCM_DiQueryResultStatus                  ZZ_KEY(DIGROUP, 0x3004) // CS, "ACCEPTED", "TOO MANY", "SYNTAX ERROR", or "PERMISSION DENIED"
#define DCM_DiQueryResultWarning                 ZZ_KEY(DIGROUP, 0x3005) // LO (optional), if there is some problem with query

// retrieve
#define DCM_DiRetrieveSequence                   ZZ_KEY(DIGROUP, 0x4001) // SQ, may contain multiple items, to contain UIDs to 
                                                                         // retrieve, server responds back with a storage request
                                                                         /// where DCM_DiStorageMethod is "REQUESTED"
#define DCM_DiRetrieveStatus                     ZZ_KEY(DIGROUP, 0x4002) // CS, if present in storage item, means failure, can be
                                                                         // "PERMISSION DENIED" or "NOT FOUND" or "BUSY"

// access request
#define DCM_DiAccessAuthorizing                  ZZ_KEY(DIGROUP, 0x5001) // PN, name of person authorizing access (deliberately no phone #!)
#define DCM_DiAccessTechnicalContact             ZZ_KEY(DIGROUP, 0x5002) // PN
#define DCM_DiAccessTechnicalContactPhone        ZZ_KEY(DIGROUP, 0x5003) // LO, phone number of technical contact
#define DCM_DiAccessMessage                      ZZ_KEY(DIGROUP, 0x5004) // LT, message to IT admin
#define DCM_DiAccessInstitutionDepartmentName    ZZ_KEY(DIGROUP, 0x5005) // LO, where modality is located
#define DCM_DiAccessEquipmentSequence            ZZ_KEY(DIGROUP, 0x5006) // SQ, insert < C.7.5.2 Enhanced General Equipment Module >
#define DCM_DiAccessPrivilegeRequestedSequence   ZZ_KEY(DIGROUP, 0x5007) // SQ, may have multiple items, containing DCM_DiNetworkService
                                                                        // and optional extras like DCM_DiStorageMethod
#define DCM_DiAccessStatus                       ZZ_KEY(DIGROUP, 0x5099) // CS, "QUEUED" (or "FAILED" if contents are invalid)

// info request
#define DCM_DiInfoEquipmentSequence              ZZ_KEY(DIGROUP, 0x6002) // SQ, insert < C.7.5.2 Enhanced General Equipment Module >
#define DCM_DiInfoLastCalibrationDateTime        ZZ_KEY(DIGROUP, 0x6003) // DT
#define DCM_DiInfoCalibrationIntervals           ZZ_KEY(DIGROUP, 0x6004) // DT VM=2, first recommended, second required
#define DCM_DiInfoCalibrationRequirements        ZZ_KEY(DIGROUP, 0x6005) // LT, info text
#define DCM_DiInfoProductionDate                 ZZ_KEY(DIGROUP, 0x6010) // DA, original production date
#define DCM_DiInfoInstallationDate               ZZ_KEY(DIGROUP, 0x6011) // DA, modality installation date at institution
#define DCM_DiInfoInstitutionDepartmentName      ZZ_KEY(DIGROUP, 0x6020) // LO, where modality is located
#define DCM_DiInfoTechnicalContact               ZZ_KEY(DIGROUP, 0x6021) // PN
#define DCM_DiServiceEventsSequence              ZZ_KEY(DIGROUP, 0x6080) // SQ, multiple items
#define DCM_DiAdverseEventsSequence              ZZ_KEY(DIGROUP, 0x6090) // SQ, multiple items

// pre-cache replication service
#define DCM_DiReplicatePermissionsSequence       ZZ_KEY(DIGROUP, 0x8005) // SQ, for pre-cache replication using "REPLICATE" service

#define DCM_DiTraceLog                           ZZ_KEY(DIGROUP, 0x0001) // CS, either "SENT" or "RECEIVED"
#define DCM_DiTraceSequence                      ZZ_KEY(DIGROUP, 0xda1a) // SQ, for debugging network communications, odd items=client

#define DCM_DiDisconnect                         ZZ_KEY(DIGROUP, 0xffff) // CS, "SUCCESS" or "FAILURE", nice way to disconnect
