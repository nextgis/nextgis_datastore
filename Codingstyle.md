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
6. The pointer asterisk and reference ampersand signs in the variable's declaration should be at left (not right), close to the type name. In the dereferencing operation, the pointer asterisk sign must be on the right, close to the variable name. In the operation of taking the address, the  ampersand signs must be on the right, close to the variable name too.
For example:
```
const char& m = 'g';
const char* name;
char** list;
Dataset* parentDS;
Dataset* const dataset;
```
but:
```
const char n = *name;
name = &m;
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
11. The destructor should be virtual for any class with virtual methods.
12. Do one initialization per line in constructor's init list. It will make search and replace much easier. First parameter or parent class in same line as function. The others at new line. The comma after the parameter.

```
GDALCADDataset::GDALCADDataset() : poCADFile(NULL)
    papoLayers(NULL),
    nLayers(0),
    poRasterDS(NULL)
```
13. All class members should be set in an initializer list or in an constructor's body.
```
explicit CADWrapperRasterBand(GDALRasterBand* poBaseBandIn) : m_poBaseBand(poBaseBandIn)
{
    m_anotherVar = complexInit();
}
```
14. Localize loop counters to the individual for loops unless you have a specific reason that they need to have such a large scope. And if so, comment why they need to be here.
15. Use C++ cast, do not use C cast.
16. Add const, where necessary.
17. Use const& for speed and safety in for-loop.
```
 for (const CADClass& cadClass : classes)
```
18. Don't use endl. http://en.cppreference.com/w/cpp/io/manip/endl : "In many implementations, standard output is line-buffered, and writing '\n' causes a flush anyway, unless std::cout.sync_with_stdio(false) was executed. In those situations, unnecessary endl only degrades the performance of file output, not standard output."
19. Fit to 80 cols.
20. Capitalize indicate and add a period to the end of the sentence. Here and all your other comments. Writing in as close to complete sentences reduces fatigue on many readers.
21. Explain what the future holds for commented out code.
22. Using brackets and operators. Brackets not separated from other letters. The operators (+,-,/,=,\*) are separated from other letters with spaces.
```
for(i = 0; i < 10; ++1)
```

# TBD

# Links

* [Google coding guide](https://google.github.io/styleguide/cppguide.html)
