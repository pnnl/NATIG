"""
Configuration helper

- Load GridLAB-D output xml file
  - Dedicated for IEEE 123 Node Test Feeder Model
- Create configuration files:
  - Helics configurations for GridLAB-D and NS3
  - Simple topology for NS3
- Create points files used for DNP3 applications

Author: Joonseok Kim
"""
import argparse
import json
import re
import os

class EndPoint:
    def __init__(self, name:str, type:str, unique=False, destination=None, info=None):
        self.name = name
        self.type = type
        self.unique = unique
        self.destination = destination
        self.info = info
    
    def json(self, as_dict=False):
        d = {}
        for key, value in vars(self).items():
            if value is not None:
                if key == "unique":
                    d["global"] = value
                else:
                    d[key] = value
        if as_dict:
            return d
        return json.dumps(d, indent=4)


class Filter:
    def __init__(self, name:str, sourcetargets=[], operation="reroute"):
        self.name = name
        self.sourcetargets = sourcetargets
        self.operation = operation
        self.properties = {}

    def set_properties(self, prop):
        self.properties = prop
    
    def set_property(self, key, value):
        self.properties[key] = value

    def json(self, as_dict=False):
        d = {}
        for key, value in vars(self).items():
            if value is not None:
                d[key] = value
        if as_dict:
            return d
        return json.dumps(d, indent=4)


class FedConfigBuilder:
    def __init__(self, name:str, loglevel=1, coreType="zmq", period=1):
        self.name = name
        self.loglevel = loglevel
        self.coreType = coreType
        self.period = period
        self.endpoints = []
        self.filters = []
    
    def add_endpoint(self, ep:EndPoint):
        self.endpoints.append(ep)
    
    def add_endpoint_with_filter(self, ep:EndPoint, filterName:str=None):
        if filterName is None:
            filterName = "filter_" + ep.name
        f = Filter(filterName, [ep.name])
        f.set_property("name", "newdestination")
        f.set_property("value", f"{self.name}/{ep.name}")
        self.endpoints.append(ep)
        self.filters.append(f)

    def json(self, as_dict=False):
        d = {}
        for key, value in vars(self).items():
            if value is not None:
                if key == "endpoints":
                    l = []
                    for ep in value:
                        l.append(ep.json(as_dict=True))
                    if len(l) > 0:
                        d[key] = l
                elif key == "filters":
                    l = []
                    for ep in value:
                        l.append(ep.json(as_dict=True))
                    if len(l) > 0:
                        d[key] = l
                else:
                    d[key] = value
        if as_dict:
            return d
        return json.dumps(d, indent=4)


def config_gridlabd(grids, federate_name="GLD"):
    # properties of interest
    properties = {
        "node":["voltage_A", "voltage_B", "voltage_C"],
        "load":["voltage_A", "voltage_B", "voltage_C"],
        "capacitor":["switchA", "switchB", "switchC", "capacitor_A", "capacitor_B", "capacitor_C"],
        "switch":[
            "status",
            "phase_A_state","phase_B_state","phase_C_state",
            "current_in_A", "current_in_B", "current_in_C"
        ],
        "regulator":[
            "status",
            # "current_in_A", "current_in_B", "current_in_C"
            "tap_A", "tap_B", "tap_C"
        ],
        "inverter_dyn":["VA_Out.real","VA_Out.imag","Pref","Qref","V_In","VA_Out"],
        # "inverter_dyn":["VA_Out.real","VA_Out.imag","IEEE_1547_trip_method","Pref","Qref","V_In","VA_Out"],
        "overhead_line":[
            "current_out_A", "current_out_B", "current_out_C",
            "current_in_A", "current_in_B", "current_in_C",
        ],
    }
    builder = FedConfigBuilder(federate_name)

    for grid in grids:
        info = {}
        for prop_type, prop in properties.items():
            if prop_type in grid:
                info = {**info, **{n: prop for n in grid[prop_type]}}
                for n in grid[prop_type]:
                    for each_prop in prop:
                        # naming convention: {group}_{gridlabd-object}${property}
                        ep = EndPoint("{0}_{1}${2}".format(grid["name"], n, each_prop), type="string", info=json.dumps({n:each_prop}))
                        builder.add_endpoint(ep)

        ep = EndPoint(grid["name"], type="string", destination=grid["dest"], info=json.dumps(info))
        builder.add_endpoint(ep)
    
    return builder


def to_metadata(builder, basedir):
    complex_number_variables = ["voltage_A","voltage_B","voltage_C","current_in_A","current_in_B","current_in_C"]
    binary_variables = ["status", "switchA", "switchB", "switchC", "phase_A_state", "phase_B_state", "phase_C_state"]
    for ep in builder.endpoints:
        if ep.destination is not None:
            with open(os.path.join(basedir, f"points_{ep.name}.csv"), "w") as f:
                info = json.loads(ep.info)
                for key, values in info.items():
                    for v in values:
                        if v in complex_number_variables:
                            f.write(f"ANALOG,{key}${v}.real,0\n")
                            f.write(f"ANALOG,{key}${v}.imag,0\n")
                        elif v in binary_variables:
                            f.write(f"BINARY,{key}${v},0\n")
                        else:
                            f.write(f"ANALOG,{key}${v},0\n")


def config_app(controlCenter, federate_name="CC"):
    builder = FedConfigBuilder(federate_name)

    ep = EndPoint(controlCenter["name"], type="string")
    builder.add_endpoint(ep)

    return builder


def create_ns3_fed(builder:FedConfigBuilder, name="ns3", loglevel=1, period=1):
    nsbuilder = FedConfigBuilder(name, loglevel=loglevel, period=period)

    for ep in builder.endpoints:
        new_ep = EndPoint(name=f"{builder.name}/{ep.name}", type=ep.type)
        nsbuilder.add_endpoint_with_filter(new_ep)
    
    return nsbuilder


def merge_ns3_fed(nsbuilder:FedConfigBuilder, builder:FedConfigBuilder):
    for ep in builder.endpoints:
        new_ep = EndPoint(name=f"{builder.name}/{ep.name}", type=ep.type)
        nsbuilder.add_endpoint_with_filter(new_ep)
    
    return nsbuilder


def network_topology(nsbuilder:FedConfigBuilder, microgrid, controlCenter, network_type="client-server"):
    topology = {}
    if network_type == "client-server":
        topology = {"controlCenter":controlCenter}
    else:
        topology = {}
    topology = {**{"microgrid":microgrid}, **topology}
    return topology

import xml.etree.ElementTree as ET

def get_gridlabd_objects(root, target, attribute, domain="powerflow",):
    names = []
    for item in root.findall(f"{domain}/{target}_list/{target}/{attribute}"):
        names.append(item.text)
    return names

#   <powerflow>
# 		<version.major>4</version.major>
# 		<version.minor>3</version.minor>
# 		<node_list>
# 			<inherits_from>powerflow_object</inherits_from>
# 			<node>
# 				<id>35</id>
# 				<rank>1</rank>
# 				<out_svc>NEVER</out_svc>
# 				<name>node_3</name>
#       <capacitor_list>
# 			<inherits_from>node</inherits_from>
# 			<capacitor>
#       <overhead_line_list>
# 			<inherits_from>line</inherits_from>
# 			<overhead_line>

def within_range(ranges, n):
    ret = False
    for r in ranges:
        if isinstance(r, tuple):
            ret = r[0] <= n <= r[1]
        else:
            ret = r == n
        if ret:
            return ret
    return ret

def vertex_pattern_match(expression, text, id_range):
    match = re.fullmatch(expression, text)
    if match:
        s = re.findall("(\d+)", text)
        return within_range(id_range, int(s[0]))
    return False

def edge_pattern_match(expression, text, id_range1, id_range2):
    match = re.fullmatch(expression, text)
    if match:
        s = re.findall("(\d+)", text)
        # print(int(s[0]), int(s[1]))
        return within_range(id_range1, int(s[0])) or within_range(id_range2, int(s[1]))
    return False

def naming_convention_v1():
    if target in ["node","load"]:
        # TODO: Set regular expression
        expression = f"(?P<vertex_type>\w+)_(?P<vertex_id>\d+)"
        match = re.fullmatch(expression)
        if match:
            vertex_type = match.group("vertex_type")
            vertex_id = match.group("vertex_id")
    


        for idx in range(len(microgrids)):
            if vertex_pattern_match(expression, n, ranges[idx]):
                if target_node_name not in microgrids[idx]:
                    microgrids[idx][target_node_name] = []
                microgrids[idx][target_node_name].append(n)
    elif target in ["switch"]:
        expression = f"sw(\d+)to(\d+)"
        for idx in range(len(microgrids)):
            if edge_pattern_match(expression, n, ranges[idx], ranges[idx]):
                if target_node_name not in microgrids[idx]:
                    microgrids[idx][target_node_name] = []
                microgrids[idx][target_node_name].append(n)
    else:
        raise Exception(f"Unknown target {target}.")


class MatchingRule:
    def __init__(self, root):
        self.root = root
    
    def get_objects(self, target, domain="powerflow"):
        return self.root.findall(f"{domain}/{target}_list/{target}")


class VertexVisitor:
    def __init__(self, vertex_type, range_mapping, expression = f"(?P<vertex_type>\w+)_(?P<vertex_id>\d+)"):
        self.expression = expression
        self.vertex_type = vertex_type
        self.range_mapping = range_mapping


    def check(self, text):
        match = re.fullmatch(self.expression, text)
        if match:
            vertex_type = match.group("vertex_type")
            vertex_id = int(match.group("vertex_id"))

            for mapping in self.range_mapping:
                if within_range(mapping["ranges"], vertex_id):
                    return mapping["name"]
        return False
    
    def naming_visit(self, xmlnode):
        name = xmlnode.find("name")
        return self.check(name.text)
    
    def parent_visit(self, xmlnode):
        name = xmlnode.find("parent")
        return self.check(name.text)

    def edge_visit(self, xmlnode):
        from_node = xmlnode.find("from")
        to_node = xmlnode.find("to")
        from_result = self.check(from_node.text)
        to_result = self.check(to_node.text)
        if from_result is None and to_result is None:
            return None
        if from_result is None:
            return to_result
        # if to_result is None:
        #     return from_result
        return from_result



def assign_object_to_group(rule, target, range_mapping, groups):
    """
    Assign a GridLAB-D object to a group (e.g., microgrid, substation)
    """

    # based on naming rule of basic vertex type
    # basic vertex type: node or load
    # inherited vertex type: capacitor (load), diesel_dg (node, load), inverter_dyn
    # edge types: switch, transformer, regulator, overhead_line, underground_line

    if target in ["node", "load"]:
        nodeVisitor = VertexVisitor(target, range_mapping)
        for xmlnode in rule.get_objects(target):
            group_name = nodeVisitor.naming_visit(xmlnode)
            if group_name:
                for group in groups:
                    if group_name == group["name"]:
                        if target not in group:
                            group[target] = []
                        group[target].append(xmlnode.find("name").text)
    elif target in ["capacitor", "diesel_dg", "inverter_dyn"]:
        nodeVisitor = VertexVisitor(target, range_mapping)
        for xmlnode in rule.get_objects(target):
            group_name = nodeVisitor.parent_visit(xmlnode)
            if group_name:
                for group in groups:
                    if group_name == group["name"]:
                        if target not in group:
                            group[target] = []
                        group[target].append(xmlnode.find("name").text)
    elif target in ["switch", "transformer", "regulator", "overhead_line", "underground_line"]:
        nodeVisitor = VertexVisitor(target, range_mapping)
        for xmlnode in rule.get_objects(target):
            group_name = nodeVisitor.edge_visit(xmlnode)
            if group_name:
                for group in groups:
                    if group_name == group["name"]:
                        if target not in group:
                            group[target] = []
                        group[target].append(xmlnode.find("name").text)
    else:
        raise Exception(f"Unknown target {target}.")



def main2(basedir=".", xmlfile="gridlabd.xml"):
    if not os.path.exists(basedir):
        os.mkdir(basedir)
    tree = ET.parse(xmlfile)
    root = tree.getroot()

    range_mapping=[
        {"name":"mg1", "ranges":[(35,51), 135, 151]}, # mg1
        {"name":"mg2", "ranges":[(101,114), 197, 300]}, # mg2
        {"name":"mg3", "ranges":[(67,100), 160, 450]}, # mg3
        {"name":"substation", "ranges":[(1,34), (52,66), (149,150), 195, 250]}, # substation
        #[195, 251, 451] outside of range?
    ]

    # object type of interest
    targets = [
        "node", "load",
        # "capacitor", "diesel_dg", "inverter_dyn",
        "switch", 
        # "transformer", 
        "regulator", 
        # "overhead_line", "underground_line"
    ]

    microgrids = [
        {
            "name":"mg1", 
            "dest":"ns3/mg1"
        },
        {
            "name":"mg2", 
            "dest":"ns3/mg2"
        },
        {
            "name":"mg3", 
            "dest":"ns3/mg3"
        },
        {
            "name":"substation",
            "dest":"ns3/substation"
        }
    ]
    controlCenter = {"name":"Monitor"}

    rule = MatchingRule(root)

    for target in targets:
        assign_object_to_group(rule, target, range_mapping, microgrids)

    gbuilder = config_gridlabd(microgrids)
    abuilder = config_app(controlCenter)
    nsbuilder = create_ns3_fed(gbuilder, period=0.1)
    nsbuilder = merge_ns3_fed(nsbuilder, abuilder)

    topology = network_topology(nsbuilder, microgrids, controlCenter)

    with open(os.path.join(basedir,"gridlabd_config.json"), "w") as f:
        f.write(gbuilder.json())
    with open(os.path.join(basedir,"cc_config.json"), "w") as f:
        f.write(abuilder.json())
    with open(os.path.join(basedir,"ns_config.json"), "w") as f:
        f.write(nsbuilder.json())
    with open(os.path.join(basedir,"grid.json"), "w") as f:
        f.write(json.dumps(topology, indent=4))

    to_metadata(gbuilder, basedir)
    

def main(xmlfile="gridlabd.xml"):
    """
    Main version for test_IEEE123_zero_voltages_NR.glm in CPS_modeling/input directory.
    """
    tree = ET.parse(xmlfile)
    root = tree.getroot()

    ranges=[
        [(35,51), 135, 151], # mg1
        [(101,114), 197, 300], # mg2
        [(67,100), 160, 450], # mg3
        [(1,34), (52,66), (149,150), 195, 250], # substation
        #[195, 251, 451] outside of range?
    ]

    targets = ["node", "load", "switch"]

    microgrids = [
        {
            "name":"mg1", 
            "dest":"ns3/mg1"
        },
        {
            "name":"mg2", 
            "dest":"ns3/mg2"
        },
        {
            "name":"mg3", 
            "dest":"ns3/mg3"
        },
        {
            "name":"substation",
            "dest":"ns3/substation"
        }
    ]
    for target in targets:
        nodes = get_gridlabd_objects(root, target, "name")
        target_node_name = f"{target}s"
        
        for n in nodes:
            if target in ["node","load"]:
                # TODO: Set regular expression
                expression = f"{target}_(\d+)"
                for idx in range(len(microgrids)):
                    if vertex_pattern_match(expression, n, ranges[idx]):
                        if target_node_name not in microgrids[idx]:
                            microgrids[idx][target_node_name] = []
                        microgrids[idx][target_node_name].append(n)
            elif target in ["switch"]:
                expression = f"sw(\d+)to(\d+)"
                for idx in range(len(microgrids)):
                    if edge_pattern_match(expression, n, ranges[idx], ranges[idx]):
                        if target_node_name not in microgrids[idx]:
                            microgrids[idx][target_node_name] = []
                        microgrids[idx][target_node_name].append(n)
            else:
                raise Exception(f"Unknown target {target}.")

    # microgrids = [
    #     {
    #         "name":"mg1", 
    #         "loads":["load_48"],
    #         "dest":"ns3/mg1"
    #     },
    #     {
    #         "name":"mg2", 
    #         "nodes":["node_300"],
    #         "dest":"ns3/mg2"
    #     },
    #     {
    #         "name":"mg3", 
    #         "nodes":["node_97"],
    #         "dest":"ns3/mg3"
    #     }
    # ]
    # for filters
    # microgrids = [
    #     {
    #         "name":"mg1", 
    #         "loads":["load_48"],
    #         "dest":"CC/Monitor"
    #     },
    #     {
    #         "name":"mg2", 
    #         "nodes":["node_300"],
    #         "dest":"CC/Monitor"
    #     },
    #     {
    #         "name":"mg3", 
    #         "nodes":["node_97"],
    #         "dest":"CC/Monitor"
    #     }
    # ]
    controlCenter = {"name":"Monitor"}

    gbuilder = config_gridlabd(microgrids)
    abuilder = config_app(controlCenter)
    nsbuilder = create_ns3_fed(gbuilder, period=0.1)
    nsbuilder = merge_ns3_fed(nsbuilder, abuilder)

    topology = network_topology(nsbuilder, microgrids, controlCenter)


    with open("gridlabd_config.json", "w") as f:
        f.write(gbuilder.json())
    with open("cc_config.json", "w") as f:
        f.write(abuilder.json())
    with open("ns_config.json", "w") as f:
        f.write(nsbuilder.json())
    with open("grid.json", "w") as f:
        f.write(json.dumps(topology, indent=4))

    to_metadata(gbuilder)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('-basedir', default=".", help="Saves results to specified folder.")

    arguments = parser.parse_args()


    # main()
    main2(basedir=arguments.basedir)
