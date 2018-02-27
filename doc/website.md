CPPAN project is located on the https://cppan.org/ site.

It uses ssl for any interactions, api calls etc. It uses http to send packages (data) to users. But hashsum checks are performed on client side and hashes are trasferred securely.

On that site you can find projects, organizations, project versions.

To add your project to CPPAN you have to:

1. Register (e.g. `cppan` account name).
1. Login
1. Create project. It will be created under your account in `private` - `pvt` root namespace. E.g. `pvt.cppan.my_very_useful_library`.
1. Add project version.

Project version sources

1. git repository
2. remote file (on some server: http, ftp)
3. local file (from your computer)

You can add either fixed version (`1.2.8`) or a branch (`master`, `develop`). Branch is updatable. Version is not.

When adding version from git, it tries to find a tag in form `prefixX.Y.Z` where `X.Y.Z` is version provided and `prefix` is custom prefix on your tags. For example, boost uses `boost-` prefix, some projects use `v` and some use empty prefix. You cannot change `X.Y.Z` delimiters. It's always `.`. So, if you want to add your project, consider changing your tag naming schema for future. 

When adding branch from git, it tries to find a branch with same name in the git repo.

Types of projects:

1. library
2. executable
3. root project - an umbrella project which can be downloaded as dependency with all its children. For example, if you write in `dependencies` `pvt.cppan.demo.boost` it will download and compile whole boost.
4. directory - an umbrella project which can not be downloaded as dependency.

You can specify custom content of `cppan.yml` on AddProjectVersion page. It helps if you're experimenting.

You can create a permanent `cppan.yml` file in your repository or an archive, so it will be used as input.

## Organizations

You can add an organization if you are representing one. Then you can add admins (can create, remove projects) and users (can  add versions of their projects).

Organizations will receive two root namespaces: `org` for projects with open source licensed and `com` for proprietary code. Private repositories both for users and orgs probably will be introduced later.
