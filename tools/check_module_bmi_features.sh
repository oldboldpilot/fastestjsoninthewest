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

findings=0
scanned=0
reported=""

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
    bad=""
    for tok in ${cmd}; do
        case "${tok}" in
            -march=*|-mtune=*) ;;
            -m[a-z]*)
                feat="${tok#-m}"
                case " ${WAIVE} " in *" ${feat} "*) continue ;; esac
                case "${feat}" in no-*|omit-*|cmodel*|stack*|red-zone|unaligned*|module*|macosx*) continue ;; esac
                case " ${bad} " in *" ${tok} "*) continue ;; esac
                bad="${bad} ${tok}"
                ;;
        esac
    done
    if [[ -n "${bad}" ]]; then
        findings=$((findings + 1))
        echo "FEATURE-ON-BMI ${src#"${PWD}/"}:${bad}"
        echo "    a consumer must now name${bad} in exactly that spelling, or the BMI will not load."
        echo "    -march= enabling the same silicon is NOT enough: it emits no -target-feature."
    fi
done < <(ninja -C "${BUILD}" -t commands all 2>/dev/null)

if [[ ${scanned} -eq 0 ]]; then
    echo "module-bmi-features: FAILED -- no module interface found in ${BUILD}."
    echo "  A gate that scanned nothing has not checked anything. Is the directory configured,"
    echo "  and does this project build C++ modules at all?"
    exit 1
fi

echo "module-bmi-features: ${scanned} module precompile(s) scanned, ${findings} carrying an explicit feature"
if [[ ${findings} -ne 0 ]]; then
    echo "module-bmi-features: FAILED -- move the feature into the baseline (-march=), where every"
    echo "  consumer inherits it, or into an implementation translation unit, which exports no BMI."
    echo "  To accept it deliberately: BMI_FEATURES_WAIVE='fma avx2' with the reason in the commit."
    exit 1
fi
echo "module-bmi-features: OK -- every module BMI demands only its baseline"
exit 0
