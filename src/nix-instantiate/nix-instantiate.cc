#include "globals.hh"
#include "shared.hh"
#include "eval.hh"
#include "eval-inline.hh"
#include "get-drvs.hh"
#include "attr-path.hh"
#include "value-to-xml.hh"
#include "util.hh"
#include "store-api.hh"
#include "common-opts.hh"
#include "help.txt.hh"

#include <map>
#include <iostream>


using namespace nix;


void printHelp()
{
    std::cout << string((char *) helpText);
}


static Expr * parseStdin(EvalState & state)
{
    startNest(nest, lvlTalkative, format("parsing standard input"));
    string s, s2;
    while (getline(std::cin, s2)) s += s2 + "\n";
    return state.parseExprFromString(s, absPath("."));
}


static Path gcRoot;
static int rootNr = 0;
static bool indirectRoot = false;


void processExpr(EvalState & state, const Strings & attrPaths,
    bool parseOnly, bool strict, Bindings & autoArgs,
    bool evalOnly, bool xmlOutput, bool location, Expr * e)
{
    if (parseOnly)
        std::cout << format("%1%\n") % *e;
    else
        foreach (Strings::const_iterator, i, attrPaths) {
            Value v;
            findAlongAttrPath(state, *i, autoArgs, e, v);
            state.forceValue(v);

            PathSet context;
            if (evalOnly)
                if (xmlOutput)
                    printValueAsXML(state, strict, location, v, std::cout, context);
                else {
                    if (strict) state.strictForceValue(v);
                    std::cout << v << std::endl;
                }
            else {
                DrvInfos drvs;
                getDerivations(state, v, "", autoArgs, drvs);
                foreach (DrvInfos::iterator, i, drvs) {
                    Path drvPath = i->queryDrvPath(state);
                    if (gcRoot == "")
                        printGCWarning();
                    else
                        drvPath = addPermRoot(*store, drvPath,
                            makeRootName(gcRoot, rootNr), indirectRoot);
                    std::cout << format("%1%\n") % drvPath;
                }
            }
        }
}


void run(Strings args)
{
    EvalState state;
    Strings files;
    bool readStdin = false;
    bool findFile = false;
    bool evalOnly = false;
    bool parseOnly = false;
    bool xmlOutput = false;
    bool xmlOutputSourceLocation = true;
    bool strict = false;
    Strings attrPaths;
    Bindings autoArgs;

    for (Strings::iterator i = args.begin(); i != args.end(); ) {
        string arg = *i++;

        if (arg == "-")
            readStdin = true;
        else if (arg == "--eval-only") {
            readOnlyMode = true;
            evalOnly = true;
        }
        else if (arg == "--parse-only") {
            readOnlyMode = true;
            parseOnly = evalOnly = true;
        }
        else if (arg == "--find-file")
            findFile = true;
        else if (arg == "--attr" || arg == "-A") {
            if (i == args.end())
                throw UsageError("`--attr' requires an argument");
            attrPaths.push_back(*i++);
        }
        else if (parseOptionArg(arg, i, args.end(), state, autoArgs))
            ;
        else if (parseSearchPathArg(arg, i, args.end(), state))
            ;
        else if (arg == "--add-root") {
            if (i == args.end())
                throw UsageError("`--add-root' requires an argument");
            gcRoot = absPath(*i++);
        }
        else if (arg == "--indirect")
            indirectRoot = true;
        else if (arg == "--xml")
            xmlOutput = true;
        else if (arg == "--no-location")
            xmlOutputSourceLocation = false;
        else if (arg == "--strict")
            strict = true;
        else if (arg[0] == '-')
            throw UsageError(format("unknown flag `%1%'") % arg);
        else
            files.push_back(arg);
    }

    if (attrPaths.empty()) attrPaths.push_back("");

    if (findFile) {
        foreach (Strings::iterator, i, files) {
            Path p = state.findFile(*i);
            if (p == "") throw Error(format("unable to find `%1%'") % *i);
            std::cout << p << std::endl;
        }
        return;
    }

    store = openStore();

    if (readStdin) {
        Expr * e = parseStdin(state);
        processExpr(state, attrPaths, parseOnly, strict, autoArgs,
            evalOnly, xmlOutput, xmlOutputSourceLocation, e);
    } else if (files.empty())
        files.push_back("./default.nix");

    foreach (Strings::iterator, i, files) {
        Expr * e = state.parseExprFromFile(lookupFileArg(state, *i));
        processExpr(state, attrPaths, parseOnly, strict, autoArgs,
            evalOnly, xmlOutput, xmlOutputSourceLocation, e);
    }

    state.printStats();
}


string programId = "nix-instantiate";
