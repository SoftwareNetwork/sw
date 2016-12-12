#### What is `pvt.cppan.demo` namespace?

`pvt.cppan` is a utility CPPAN account and `pvt.cppan.demo` contains many projects, but they're not official. You could use them as a starting point in uploading your packages. And official packages will be in `pvt.your_name.*` namespace. Also for orgs: `org.boost.*`, `org.google.*`, `org.facebook.*`, `com.ibm.*`.

#### Where does cppan download/generate stuff?

By default, in your user home directory. Like `$HOME/.cppan/storage` on *nix or `%USERPROFILE%/.cppan/storage` on Windows.

#### How to change that directory?

Open `$HOME/.cppan/cppan.yml`, add `storage_dir: your/favourite/dir/for/cppan`.
