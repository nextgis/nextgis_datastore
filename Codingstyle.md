# Intro
This is the coding style

# Main rules
1. All includes sort alphabetically (may be grouped, i.e. stl, gdal, this project)
2. Don't use using namespace for std
3. Initialization of float/double should be a real number (i.e. float a = 1.0)
4. Getters, setters or one line functions put in header. Other in sources.
5. If brackets ('{' and '}') use in function in one place, should use in another:

For example
```
void test(int type)
{
    if(type == 1) {
        int x = 1
    }
    else <-- Wrong - need {
        int y = 1;
}
```

6. The pointer star should be at right (not left).

For example:
```
const char *name
char **list
Dataset *parentDS
but:
Dataset * const dataset
```

7. Variable name and functions - use camel style from lower case
8. Class name - from upper case
9. In getters try not use get

For example:
```
size_t getFeatureCount() <-- Wrong
size_t featureCount() <-- Right
```

10. For smart pointers use Ptr and UPtr and WPtr postfix

# TBD

# Links

* [Google coding guide](https://google.github.io/styleguide/cppguide.html)
