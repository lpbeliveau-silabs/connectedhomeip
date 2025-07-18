# Copyright (c) 2022 Project CHIP Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import logging
import os
import xml.etree.ElementTree
from dataclasses import dataclass
from enum import Enum, auto
from typing import List, MutableMapping, Optional, Tuple, Union

from lark import Lark
from lark.visitors import Discard, Transformer, v_args

from .type_definitions import (AttributeRequirement, ClusterAttributeDeny, ClusterCommandRequirement, ClusterRequirement,
                               ClusterValidationRule, RequiredAttributesRule, RequiredCommandsRule)


class ElementNotFoundError(Exception):
    def __init__(self, name):
        super().__init__(f"Could not find {name}")


def parseNumberString(n: str) -> int:
    if n.startswith('0x'):
        return int(n[2:], 16)
    else:
        return int(n)


@dataclass
class RequiredAttribute:
    name: str
    code: int


@dataclass
class RequiredCommand:
    name: str
    code: int


@dataclass
class DecodedCluster:
    name: str
    code: int
    required_attributes: List[RequiredAttribute]
    required_commands: List[RequiredCommand]


class ClusterActionEnum(Enum):
    REQUIRE = auto()
    REJECT = auto()


@dataclass
class ServerClusterRequirement:
    action: ClusterActionEnum
    id: Union[str, int]


def _isRequired(attr: xml.etree.ElementTree.Element) -> bool:

    # Check for optional attributes in the old "optional" element format
    if 'optional' in attr.attrib and attr.attrib['optional'] == 'true':
        return False

    # Check for optionalConform inside the element
    if attr.find('optionalConform') is not None:
        return False

    # Check for provisional elements
    if 'apiMaturity' in attr.attrib and attr.attrib['apiMaturity'] == 'provisional':
        return False

    # mandatory is a marker, as long as the mandatory is not
    # turned into an optional by being controlled by a feature
    mandatory_element = attr.find('mandatoryConform')

    if mandatory_element is None:
        return True

    return mandatory_element.find('feature') is None


def DecodeClusterFromXml(element: xml.etree.ElementTree.Element):
    if element.tag != 'cluster':
        logging.error("Not a cluster element: %r" % element)
        return None

    # cluster elements contain among other children
    #  - name (general name for this cluster)
    #  - code (unique identifier, may be hex or numeric)
    #  - attribute with side, code and optional attributes
    try:
        name = element.find('name')
        if name is None or not name.text:
            raise ElementNotFoundError('name')

        name = name.text.replace(' ', '')
        required_attributes = []
        required_commands = []

        for attr in element.findall('attribute'):
            if attr.attrib['side'] != 'server':
                continue

            if not _isRequired(attr):
                continue

            # when introducing access controls, the content of attributes may either be:
            # <attribute ...>myName</attribute>
            # or
            # <attribute ...><description>myName</description><access .../>...</attribute>
            attr_name = attr.get("name")
            if not attr_name:
                description = attr.find('description')
                if description is not None:
                    attr_name = description.text

            required_attributes.append(
                RequiredAttribute(
                    name=attr_name,
                    code=parseNumberString(attr.attrib['code'])
                ))

        for cmd in element.findall('command'):
            if cmd.attrib['source'] != 'client':
                continue

            if 'optional' in cmd.attrib and cmd.attrib['optional'] == 'true':
                continue

            if 'apiMaturity' in cmd.attrib and cmd.attrib['apiMaturity'] == 'provisional':
                continue

            required_commands.append(RequiredCommand(
                name=cmd.attrib["name"], code=parseNumberString(cmd.attrib['code'])))

        code = element.find('code')
        if code is None:
            raise Exception("Failed to find cluster code")

        return DecodedCluster(
            name=name,
            code=parseNumberString(code.text),
            required_attributes=required_attributes,
            required_commands=required_commands
        )
    except Exception:
        logging.exception("Failed to decode cluster %r" % element)
        return None


def ClustersInXmlFile(path: str):
    logging.info("Loading XML from %s" % path)

    # root is expected to be just a "configurator" object
    configurator = xml.etree.ElementTree.parse(path).getroot()
    for child in configurator:
        if child.tag != 'cluster':
            continue
        yield child


class LintRulesContext:
    """Represents a context for loadint lint rules.

    Handles:
       - loading referenced files (matter xml definitions)
       - adding linter rules as data is parsed
       - Looking up identifiers for various rules
    """

    def __init__(self):
        self._required_attributes_rule = RequiredAttributesRule(
            "Required attributes")
        self._cluster_validation_rule = ClusterValidationRule(
            "Cluster validation")
        self._required_commands_rule = RequiredCommandsRule(
            "Required commands")

        # Map cluster names to the underlying code
        self._cluster_codes: MutableMapping[str, int] = {}

    def GetLinterRules(self):
        return [self._required_attributes_rule, self._required_commands_rule, self._cluster_validation_rule]

    def RequireAttribute(self, r: AttributeRequirement):
        self._required_attributes_rule.RequireAttribute(r)

    def Deny(self, what: ClusterAttributeDeny):
        self._required_attributes_rule.Deny(what)

    def FindClusterCode(self, name: str) -> Optional[Tuple[str, int]]:
        if name not in self._cluster_codes:
            # Name may be a number. If this can be parsed as a number, accept it anyway
            try:
                return "ID_%s" % name, parseNumberString(name)
            except ValueError:
                logging.error("UNKNOWN cluster name %s" % name)
                logging.error("Known names: %s" %
                              (",".join(self._cluster_codes.keys()), ))
                return None
        else:
            return name, self._cluster_codes[name]

    def RequireClusterInEndpoint(self, name: str, code: int):
        """Mark that a specific cluster is always required in the given endpoint
        """
        cluster_info = self.FindClusterCode(name)
        if not cluster_info:
            return

        name, cluster_code = cluster_info

        self._cluster_validation_rule.RequireClusterInEndpoint(ClusterRequirement(
            endpoint_id=code,
            cluster_code=cluster_code,
            cluster_name=name,
        ))

    def RejectClusterInEndpoint(self, name: str, code: int):
        """Mark that a specific cluster is always rejected in the given endpoint
        """
        cluster_info = self.FindClusterCode(name)
        if not cluster_info:
            return

        name, cluster_code = cluster_info

        self._cluster_validation_rule.RejectClusterInEndpoint(ClusterRequirement(
            endpoint_id=code,
            cluster_code=cluster_code,
            cluster_name=name,
        ))

    def LoadXml(self, path: str):
        """Load XML data from the given path and add it to
           internal processing. Adds attribute requirement rules
           as needed.
        """
        for cluster in ClustersInXmlFile(path):
            decoded = DecodeClusterFromXml(cluster)

            if not decoded:
                continue

            self._cluster_codes[decoded.name] = decoded.code

            for attr in decoded.required_attributes:
                self._required_attributes_rule.RequireAttribute(AttributeRequirement(
                    code=attr.code, name=attr.name, filter_cluster=decoded.code))

            for cmd in decoded.required_commands:
                self._required_commands_rule.RequireCommand(
                    ClusterCommandRequirement(
                        cluster_code=decoded.code,
                        command_code=cmd.code,
                        command_name=cmd.name
                    ))


class LintRulesTransformer(Transformer):
    """
    A transformer capable to transform data parsed by Lark according to
    lint_rules_grammar.lark.
    """

    def __init__(self):
        self.context = LintRulesContext()

    def positive_integer(self, tokens):
        """Numbers in the grammar are integers or hex numbers.
        """
        if len(tokens) != 1:
            raise Exception("Unexpected argument counts")

        return parseNumberString(tokens[0].value)

    @v_args(inline=True)
    def negative_integer(self, value):
        return -value

    @v_args(inline=True)
    def integer(self, value):
        return value

    def id(self, tokens):
        """An id is a string containing an identifier
        """
        if len(tokens) != 1:
            raise Exception("Unexpected argument counts")
        return tokens[0].value

    def ESCAPED_STRING(self, s):
        # handle escapes, skip the start and end quotes
        return s.value[1:-1].encode('utf-8').decode('unicode-escape')

    def start(self, instructions):
        # At this point processing is considered done, return all
        # linter rules that were found
        return self.context.GetLinterRules()

    def instruction(self, instruction):
        return Discard

    def all_endpoint_rule(self, rules: List[Union[AttributeRequirement, ClusterAttributeDeny]]):
        for rule in rules:
            if type(rule) is AttributeRequirement:
                self.context.RequireAttribute(rule)
            elif type(rule) is ClusterAttributeDeny:
                self.context.Deny(rule)
            else:
                raise Exception("Unkown endpoint requirement: %r" % rule)

        return Discard

    @v_args(inline=True)
    def load_xml(self, path):
        if os.path.exists(path):
            self.context.LoadXml(path)

    @v_args(inline=True)
    def required_global_attribute(self, name, code):
        return AttributeRequirement(code=code, name=name)

    @v_args(inline=True)
    def specific_endpoint_rule(self, code, *requirements):
        for requirement in requirements:
            if requirement.action == ClusterActionEnum.REQUIRE:
                self.context.RequireClusterInEndpoint(requirement.id, code)
            elif requirement.action == ClusterActionEnum.REJECT:
                self.context.RejectClusterInEndpoint(requirement.id, code)
            else:
                raise Exception("Unexpected requirement action %r" %
                                requirement.action)

        return Discard

    @v_args(inline=True)
    def required_server_cluster(self, id):
        return ServerClusterRequirement(ClusterActionEnum.REQUIRE, id)

    @v_args(inline=True)
    def rejected_server_cluster(self, id):
        return ServerClusterRequirement(ClusterActionEnum.REJECT, id)

    @v_args(inline=True)
    def denylist_cluster_attribute(self, cluster_id, attribute_id):
        return ClusterAttributeDeny(cluster_id, attribute_id)


class Parser:
    def __init__(self):
        self.parser = Lark.open(
            'lint_rules_grammar.lark', rel_to=__file__, parser='lalr',
            propagate_positions=True, maybe_placeholders=True)

    def parse(self, file: str):
        data = LintRulesTransformer().transform(
            self.parser.parse(file))
        return data


def CreateParser():
    """
    Generates a parser that will process a ".matter" file into a IDL
    """
    return Parser()
