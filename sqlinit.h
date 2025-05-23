const char *sqlinit =
"BEGIN TRANSACTION;"
"CREATE TABLE IF NOT EXISTS instances(filename text primary key not null, sopclassuid text, instanceuid text unique, size int default -1, lastmodified text not null, seriesuid text, "
"	volume int default 0, splitcause text default null);"
"CREATE TABLE IF NOT EXISTS series(seriesuid text primary key not null, modality text, seriesid text, studyuid text, annotation text default null);"
"CREATE TABLE IF NOT EXISTS studies(studyuid text primary key not null, patientsname text, accessionnumber text, birthdate text, patientsid text);"
"CREATE TABLE IF NOT EXISTS accessrequests(uid text primary key not null, authorizing text not null, technicalcontactname text, technicalcontactphone text,"
"	message text, institution text, manufacturer text, modelname text, serialnumber text, softwareversion text, aetitle text not null, permissions text,"
"	ip text, port int, servicetechnician text, servicephone text, publickey text);"
"CREATE TABLE IF NOT EXISTS modalities(aetitle primary key not null, permissions text, ip text, port int, institution text, manufacturer text, modelname text,"
"	serialnumber text, servicetechnician text, servicephone text, publickey text);"
"COMMIT;"
;
