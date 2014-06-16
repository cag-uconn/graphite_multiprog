#include <db.h>
#include <string>

using std::string;

namespace DBUtils
{

void initializeEnv();
void initialize(DB*& db, const string& db_name, const string& lib_name);
void shutdownEnv();
void shutdown(DB*& db);

int getRecord(DB*& db, DBT& key, DBT& data);
int putRecord(DB*& db, DBT& key, DBT& data);

}
