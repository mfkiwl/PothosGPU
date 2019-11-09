#!/usr/bin/env python

import datetime
from mako.template import Template
import mako.exceptions
import os
import sys
import yaml

ScriptDir = os.path.dirname(__file__)
OutputDir = os.path.abspath(sys.argv[1])
Now = datetime.datetime.now()

FactoryTemplate = None
BlockExecutionTestAutoTemplate = None

prefix = """// Copyright (c) 2019-{0} Nicholas Corgan
// SPDX-License-Identifier: BSD-3-Clause

//
// This file was auto-generated on {1}.
//
""".format(Now.year, Now)

def populateTemplates():
    global FactoryTemplate
    global BlockExecutionTestAutoTemplate

    factoryFunctionTemplatePath = os.path.join(ScriptDir, "Factory.mako.cpp")
    with open(factoryFunctionTemplatePath) as f:
        FactoryTemplate = f.read()

    blockExecutionTestAutoTemplatePath = os.path.join(ScriptDir, "BlockExecutionTestAuto.mako.cpp")
    with open(blockExecutionTestAutoTemplatePath) as f:
        BlockExecutionTestAutoTemplate = f.read()

def processYAMLFile(yamlPath):
    yml = None
    with open(yamlPath) as f:
        yml = yaml.load(f.read())

    if not yml:
        raise RuntimeError("No YAML found in {0}".format(yamlPath))

    return yml

DICT_ENTRY_KEYS = dict(
    supportInt="int=1",
    supportUInt="uint=1",
    supportFloat="float=1",
    supportComplexFloat="cfloat=1"
)

# Operates in-place
def generateDTypeDictEntries(supportedTypes):
    if "supportAll" in supportedTypes:
        supportedTypes["dtypeString"] = "int=1,uint=1,float=1,cfloat=1"
        supportedTypes["defaultType"] = "float64"
    else:
        supportedTypes["dtypeString"] = ",".join([DICT_ENTRY_KEYS[key] for key in DICT_ENTRY_KEYS if key in supportedTypes])
        supportedTypes["defaultType"] = "float64" if ("float=1" in supportedTypes["dtypeString"] and "cfloat=1" not in supportedTypes["dtypeString"]) else "{0}64".format(supportedTypes["dtypeString"].split("=")[0].split(",")[0]).replace("cfloat64", "complex_float64")

def generateFactory(blockYAML):
    # Generate the type support strings here (easier to do than in the Mako
    # template files).
    for category,blocks in blockYAML.items():
        for block in blocks:
            for key in ["supportedTypes", "supportedInputTypes", "supportedOutputTypes"]:
                if key in block:
                    generateDTypeDictEntries(block[key])

    try:
        rendered = Template(FactoryTemplate).render(
                       oneToOneBlocks=blockYAML["OneToOneBlocks"],
                       scalarOpBlocks=blockYAML["ScalarOpBlocks"],
                       singleOutputSources=blockYAML["SingleOutputSources"],
                       twoToOneBlocks=blockYAML["TwoToOneBlocks"])
    except:
        print(mako.exceptions.text_error_template().render())

    output = "{0}\n{1}".format(prefix, rendered)

    outputFilepath = os.path.join(OutputDir, "Factory.cpp")
    with open(outputFilepath, 'w') as f:
        f.write(output)

# TODO: make OneToOneBlock test support different types
def generateBlockExecutionTest(blockYAML):
    sfinaeMap = dict(
        Integer="Int",
        UnsignedInt="UInt",
        Float="Float",
        Complex="ComplexFloat"
    )

    try:
        rendered = Template(BlockExecutionTestAutoTemplate).render(
                       oneToOneBlocks=blockYAML["OneToOneBlocks"],
                       scalarOpBlocks=blockYAML["ScalarOpBlocks"],
                       singleOutputSources=blockYAML["SingleOutputSources"],
                       twoToOneBlocks=blockYAML["TwoToOneBlocks"],
                       sfinaeMap=sfinaeMap)
    except:
        print(mako.exceptions.text_error_template().render())

    output = "{0}\n{1}".format(prefix, rendered)

    outputFilepath = os.path.join(OutputDir, "BlockExecutionTestAuto.cpp")
    with open(outputFilepath, 'w') as f:
        f.write(output)

if __name__ == "__main__":
    populateTemplates()

    yamlFilepath = os.path.join(ScriptDir, "Blocks.yaml")
    blockYAML = processYAMLFile(yamlFilepath)

    generateFactory(blockYAML)
    generateBlockExecutionTest(blockYAML)
