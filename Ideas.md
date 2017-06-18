# Settings structure

settings/catalog/show_extensions
settings/catalog/show_hidden
settings/catalog/root_items/
settings/catalog/factories/shp_factory/enable
settings/catalog/factories/shp_factory/options
settings/catalog/factories/mi_factory/enable
settings/crypto/key
settings/crypto/alg
settings/network/timeout
settings/network/use_gzip
settings/network/proxy_url
settings/network/proxy_port
settings/map/background

# Settings folder contents

settings.json
fs_connections
db_connections
web_connections

# Plugin architecture

```cpp
void *CPLGetSymbol( const char * pszLibrary, const char * pszSymbolName )

extern "C" void* create_object(int type)
{
    switch(type)
    {
    case CATALOG_OBJECT_FACTORY:
        return new MyClass;
    case ROOT_ITEM:
        return new MyClass;
    default:
        return nullptr;
    }
}

extern "C" void destroy_object( void* object )
{
  delete object;
}
```

Details on [this link](http://stackoverflow.com/a/497158/2901140)
