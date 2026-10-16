#!/usr/bin/env bash
# A module target must not name a target feature its baseline already provides.
#
# @author Olumuyiwa Oluwasanmi
#
# WHY. Clang records the EXPLICIT target-feature list in a BMI, and the compatibility check is
# ONE-DIRECTIONAL: a BMI naming `+fma` is refused by any translation unit that does not name it
# too, while a BMI naming FEWER features loads happily in a consumer that names more. `-march=` is
# not interchangeable with `-m<feature>` here -- `-march=x86-64-v3` enables FMA and defines
# `__FMA__`, yet emits no `-target-feature +fma` to cc1, so a consumer on the IDENTICAL baseline,
# spelled that way, is told it "is not" compiled with `+fma` and every name in the module
# disappears.
#
# So a redundant `-m<feature>` on a module target is not free: it is exported as a requirement on
# every consumer, in that exact spelling. Measured twice. On 2026-10-14 this repository's own
# benchmarks and modules disagreed and `ninja` ended with 20 errors. The fix raised the modules to
# match the benchmarks, which cured the inside and broke the outside: on 2026-10-16 it stopped
# ThinButQuickWebFramework building at all, at `src/ads/answer.cppm`, on the same baseline.
#
# WHAT IT CHECKS. Every command in a configured build directory that precompiles a C++ module, for
# `-m<feature>` flags. `-march=` and `-mtune=` are the baseline and are expected; anything else is
# a finding, because a feature worth having belongs either in the baseline (where every consumer
# gets it from `-march`) or in an implementation translation unit (which exports no BMI).
#
# A line may carry an inline waiver: `bmi-features-ok: <reason>` in the target's CMake comment is
# NOT read -- a waiver here is deliberate and explicit, via BMI_FEATURES_WAIVE.
set -u

BUILD="${1:-}"
if [[ -z "${BUILD}" || ! -f "${BUILD}/build.ninja" ]]; then
    echo "usage: $0 <configured ninja build dir>"
    echo "  (a configured build dir is required: this reads the commands that will actually run,"
    echo "   not what the CMake text appears to say)"
    exit 2
fi

# Features an opt-in is allowed to add, named explicitly by whoever turned it on.
WAIVE="${BMI_FEATURES_WAIVE:-}"

# The compiler the build is configured with, so the probe answers for the toolchain in use rather
# than for whatever happens to be first on PATH.
CXX_FOR_PROBE="$(awk -F= '/^CMAKE_CXX_COMPILER:/{print $2}' "${BUILD}/CMakeCache.txt" 2>/dev/null)"
[[ -x "${CXX_FOR_PROBE}" ]] || CXX_FOR_PROBE="$(command -v clang++ || command -v c++)"
if [[ ! -x "${CXX_FOR_PROBE}" ]]; then
    echo "module-bmi-features: FAILED -- no C++ compiler to probe the baseline with; the"
    echo "  redundant-versus-above-baseline distinction cannot be made without one, and guessing"
    echo "  it from an ISA table is how this check would go stale."
    exit 2
fi


# IS THIS FEATURE ALREADY IMPLIED BY THE COMMAND'S OWN BASELINE? The compiler is the oracle, not a
# hand-kept ISA table: `-m<feat>` is REDUNDANT when compiling with the command's `-march=` alone
# already defines the feature's predefined macro. The distinction is the whole point of this gate.
#
#   REDUNDANT (a defect): `-mfma` beside `-march=x86-64-v3`. v3 enables FMA already, so the flag
#   buys no instruction -- and yet it is recorded in the BMI as a demand every consumer must spell.
#   That is cost with no benefit, and it is what broke a downstream repository on 2026-10-16.
#
#   ABOVE THE BASELINE (a decision): `-maes`, `-mpclmul` beside the same `-march`. x86-64-v3 does
#   NOT include AES-NI or PCLMUL, so these genuinely add instructions and genuinely raise the floor
#   for every consumer of the BMI. That may be exactly what the author wants; it must be said out
#   loud with BMI_FEATURES_WAIVE rather than discovered by a consumer whose build stops.
implied_by_baseline() { # feature, march-and-mtune flags
    local feat="$1"; shift
    local macro
    macro="__$(printf '%s' "${feat}" | tr 'a-z.' 'A-Z_')__"
    "${CXX_FOR_PROBE}" "$@" -dM -E -x c++ /dev/null 2>/dev/null | grep -q "define ${macro}\b"
}

findings=0
scanned=0
reported=""
above=0

# HOW MODULE TRANSLATION UNITS ARE FOUND, and why not from the target list. A C++ module's BMI is
# produced through DYNDEP: the .pcm is discovered while building, so `ninja -t targets all` lists
# only the module outputs a generator wrote down statically -- here just the std modules. The first
# version of this gate used that list, reported 4 precompiles, and PASSED on the very defect it was
# written for, because `fastjson.pcm` was never among them. So the commands are the source of
# truth: every compile whose source is a module interface, which is what actually writes a BMI.
while IFS= read -r cmd; do
    case "${cmd}" in
        *.cppm*|*-fmodule-output*|*--precompile*) ;;
        *) continue ;;
    esac
    # The source being compiled, for the report; a command with no .cppm is a consumer, not a
    # producer, and consumers are allowed to name as many features as they like.
    src=""
    for tok in ${cmd}; do
        case "${tok}" in *.cppm) src="${tok}" ;; esac
    done
    [[ -z "${src}" ]] && continue
    case " ${reported} " in *" ${src} "*) continue ;; esac
    reported="${reported} ${src}"
    scanned=$((scanned + 1))
    baseline=""
    for tok in ${cmd}; do
        case "${tok}" in -march=*|-mtune=*) baseline="${baseline} ${tok}" ;; esac
    done
    redundant=""
    raises=""
    for tok in ${cmd}; do
        case "${tok}" in
            -march=*|-mtune=*) continue ;;
            -m[a-z]*) ;;
            *) continue ;;
        esac
        feat="${tok#-m}"
        case " ${WAIVE} " in *" ${feat} "*) continue ;; esac
        case "${feat}" in no-*|omit-*|cmodel*|stack*|red-zone|unaligned*|module*|macosx*|arch=*|tune=*) continue ;; esac
        case " ${redundant} ${raises} " in *" ${tok} "*) continue ;; esac
        if [[ -n "${baseline}" ]] && implied_by_baseline "${feat}" ${baseline}; then
            redundant="${redundant} ${tok}"
        else
            raises="${raises} ${tok}"
        fi
    done
    if [[ -n "${redundant}" ]]; then
        findings=$((findings + 1))
        echo "REDUNDANT-ON-BMI ${src#"${PWD}/"}:${redundant}"
        echo "    ${baseline# } already enables these, so the flags add no instruction -- but they ARE"
        echo "    recorded in the BMI, and every consumer must then name them in the same spelling."
        echo "    Cost with no benefit: drop them and let -march= carry the baseline."
    fi
    if [[ -n "${raises}" ]]; then
        above=$((above + 1))
        echo "RAISES-BASELINE ${src#"${PWD}/"}:${raises}"
        echo "    not implied by ${baseline# }, so these genuinely add instructions AND genuinely"
        echo "    require every consumer of this BMI to name them. That may be intended; say so with"
        echo "    BMI_FEATURES_WAIVE and the reason, so a consumer does not discover it as a build stop."
    fi
done < <(ninja -C "${BUILD}" -t commands all 2>/dev/null)

if [[ ${scanned} -eq 0 ]]; then
    echo "module-bmi-features: FAILED -- no module interface found in ${BUILD}."
    echo "  A gate that scanned nothing has not checked anything. Is the directory configured,"
    echo "  and does this project build C++ modules at all?"
    exit 1
fi

echo "module-bmi-features: ${scanned} module interface(s) scanned, ${findings} with a REDUNDANT feature, ${above} raising the baseline"
if [[ ${findings} -ne 0 ]]; then
    echo "module-bmi-features: FAILED -- a redundant -m<feature> on a module interface is a demand"
    echo "  on every consumer bought with no instruction in return. Drop it; -march= already has it."
    exit 1
fi
if [[ ${above} -ne 0 ]]; then
    echo "module-bmi-features: OK, with ${above} interface(s) deliberately above the baseline (listed"
    echo "  above). Nothing redundant is exported. Waive them by name once the decision is recorded."
    exit 0
fi
echo "module-bmi-features: OK -- every module BMI demands only its baseline"
exit 0
