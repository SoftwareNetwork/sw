void build(Solution &sln)
{
    auto &s = sln.addDirectory("demo");
    auto &sqlite3 = s.addTarget<LibraryTarget>("sqlite3", "3.21.0");
    sqlite3.Source = RemoteFile("http://www.sqlite.org/2017/sqlite-amalgamation-{M}{m:02}{p:02}00.zip");
    sqlite3.fetch();
    sqlite3 += "sqlite3.[hc]"_r;
    sqlite3 += "sqlite3ext.h";
    sqlite3.ApiName = "SQLITE_API";
}
