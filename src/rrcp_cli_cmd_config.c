/*
    This file is part of OpenRRCP

    OpenRRCP is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    OpenRRCP is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with OpenRRCP; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    ---

    Some support can be found at: http://openrrcp.org.ru/
*/

#include <stdlib.h>
#include <string.h>
#include "../lib/libcli.h"
#include "rrcp_config.h"
#include "rrcp_lib.h"
#include "rrcp_io.h"
#include "rrcp_switches.h"
#include "rrcp_cli_cmd_show.h"

int cmd_config_version(struct cli_def *cli, char *command, char *argv[], int argc)
{
    if (argc==1){
	if (strcmp(switchtypes[switchtype].chip_name,argv[0])!=0){
	    cli_print(cli, "%%SYS-4-CONFIG_MISMATCH: Configuration from chip '%s' may not be correctly understood on current chip '%s'", argv[0], switchtypes[switchtype].chip_name);
	}
    }
    return CLI_OK;
}

int cmd_config_hostname(struct cli_def *cli, char *command, char *argv[], int argc)
{
    if (argc==1){
	if (strcmp(argv[0],"?")==0){
	    cli_print(cli, "Set system's network host name");
	}else{
	    cli_set_hostname(cli,argv[0]);
	}
    }else{
	cli_print(cli, "%% Invalid hostname specification");
    }
    return CLI_OK;
}

int cmd_config_mac_aging(struct cli_def *cli, char *command, char *argv[], int argc)
{
    if (argc==1){
	if (strcmp(argv[0],"?")==0){
	    cli_print(cli, "  <0>        Enter 0 to disable aging");
	    cli_print(cli, "  <12|300>   Aging time in seconds");
	}else if (strcmp(argv[0],"0")==0){
	    swconfig.alt_config.s.config.mac_aging_disable=1;
	    swconfig.alt_config.s.config.mac_aging_fast=0;
	}else if (strcmp(argv[0],"12")==0){
	    swconfig.alt_config.s.config.mac_aging_disable=0;
	    swconfig.alt_config.s.config.mac_aging_fast=1;
	}else if (strcmp(argv[0],"300")==0){
	    swconfig.alt_config.s.config.mac_aging_disable=0;
	    swconfig.alt_config.s.config.mac_aging_fast=0;
	}else{
	    cli_print(cli, "%% Invalid aging time '%s', can be only 0,12 or 300.",argv[0]);
	    return CLI_ERROR;
	}
	rtl83xx_setreg16(0x0300,swconfig.alt_config.raw);
    }
    return CLI_OK;
}

int cmd_config_ip_igmp_snooping(struct cli_def *cli, char *command, char *argv[], int argc)
{
    if (argc>0){
	if (strcmp(argv[0],"?")==0){
	    cli_print(cli, "  <cr>");
	}else{
	    cli_print(cli, "%% Invalid input detected.");
	}
    }else{
	if (strcasecmp(command,"ip igmp snooping")==0)    swconfig.alt_igmp_snooping.config.en_igmp_snooping=1;
	if (strcasecmp(command,"no ip igmp snooping")==0) swconfig.alt_igmp_snooping.config.en_igmp_snooping=0;
	rtl83xx_setreg16(0x0308,swconfig.alt_igmp_snooping.raw);
    }
    return CLI_OK;
}

int cmd_config_rrcp(struct cli_def *cli, char *command, char *argv[], int argc)
{
    if (argc>0){
	if (strcmp(argv[0],"?")==0){
	    cli_print(cli, "  <cr>");
	}else{
	    cli_print(cli, "%% Invalid input detected.");
	}
    }else{
	if (strcasecmp(command,"rrcp enable")==0)    swconfig.rrcp_config.config.rrcp_disable=0;
	if (strcasecmp(command,"no rrcp enable")==0) swconfig.rrcp_config.config.rrcp_disable=1;
	if (strcasecmp(command,"rrcp echo enable")==0)    swconfig.rrcp_config.config.echo_disable=0;
	if (strcasecmp(command,"no rrcp echo enable")==0) swconfig.rrcp_config.config.echo_disable=1;
	if (strcasecmp(command,"rrcp loop-detect enable")==0)    swconfig.rrcp_config.config.loop_enable=1;
	if (strcasecmp(command,"no rrcp loop-detect enable")==0) swconfig.rrcp_config.config.loop_enable=0;
	rtl83xx_setreg16(0x0200,swconfig.rrcp_config.raw);
    }
    return CLI_OK;
}

int cmd_config_rrcp_authkey(struct cli_def *cli, char *command, char *argv[], int argc)
{
    if (argc==1){
	if (argv[0][strlen(argv[0])-1]=='?'){
	    cli_print(cli, "0000-ffff           New RRCP Authentication key (hex)");
	}else{
	    int new_authkey;
	    
	    if (sscanf(argv[0],"%04x",&new_authkey)){
		rtl83xx_setreg16(0x209,new_authkey);
		authkey=new_authkey;
	    }else{
		cli_print(cli, "%% ERROR: Invalid authkey specification: '%s'",argv[0]);
		return CLI_ERROR;
	    }
	}
    }else{
	cli_print(cli, "%% Invalid input detected.");
	return CLI_ERROR;
    }
    return CLI_OK;
}

int cmd_config_vlan(struct cli_def *cli, char *command, char *argv[], int argc)
{
    if (argc>0){
	if (strcasecmp(command,"no vlan")==0) {
	    if (argv[0][strlen(argv[0])-1]=='?'){
		cli_print(cli, "  <1-4095>             Remove exiting vlan from switch");
		cli_print(cli, "  <cr>");
	        return CLI_ERROR;
	    }else{
		int vid,vi,port,port_phys,cnt;
		struct t_str_number_list vidlist;

		cnt = str_number_list_init(argv[0], &vidlist);
		if ((cnt <= 0) || cnt > 32) {
		   cli_print(cli, "%% ERROR: Invalid vlan list: '%s'",argv[0]);
		   return CLI_ERROR;
		}
		for (cnt=0; str_number_list_get_next(&vidlist, &vid) == 0; cnt++) {
		   if (vid < 0 || vid > 4095) {
		      cli_print(cli, "%% ERROR: Incorrect VLAN index: %i", vid);
		      return CLI_ERROR;
		   }
		}

		str_number_list_init(argv[0], &vidlist);
		while (str_number_list_get_next(&vidlist, &vid) == 0) {
		   vi=find_vlan_index_by_vid(vid);
		   if (vi>=0){
		      swconfig.vlan_entry.bitmap[vi]=0;
		      if (vid>1){
			 swconfig.vlan_vid[vi]=0;
		      }
		   }
		   for(port=1;port<=switchtypes[switchtype].num_ports;port++){
		      port_phys=map_port_number_from_logical_to_physical(port);
		      if (swconfig.vlan.s.port_vlan_index[port_phys]==vi){
			 swconfig.vlan.s.port_vlan_index[port_phys]=find_vlan_index_by_vid(1);
			 swconfig.vlan_entry.bitmap[find_vlan_index_by_vid(1)]|=1<<port_phys;
		      }
		   }
		}
		rrcp_config_commit_vlan_to_switch();
		return CLI_OK;
	    }
	}
	if (strcmp(argv[0],"?")==0){
	    cli_print(cli, "  <cr>");
	    return CLI_ERROR;
	}else{
	    cli_print(cli, "%% Invalid input detected.");
	    return CLI_ERROR;
	}
    }else{
	if (strcasecmp(command,"no vlan")==0) {
	    swconfig.vlan.s.config.dot1q=0;
	    swconfig.vlan.s.config.enable=0;
	    swconfig.vlan.s.config.drop_untagged_frames=0;
	    swconfig.vlan.s.config.ingress_filtering=0;
	    rrcp_config_commit_vlan_to_switch();
	}
	if (strcasecmp(command,"vlan portbased")==0) {
	    int i,port,port_phys;
	    cli_print(cli, "%% WARNING: Portbased VLANs are not fully supported in this software yet.");
	    for(i=1;i<32;i++){
	        swconfig.vlan_vid[i]=i+1;
	    }
	    for(port=1;port<=switchtypes[switchtype].num_ports;port++){
	        port_phys=map_port_number_from_logical_to_physical(port);
		swconfig.vlan.s.port_vlan_index[port_phys]=0;
	    }
	    swconfig.vlan_port_insert_vid.bitmap=0;
	    swconfig.vlan.s.config.dot1q=0;
	    swconfig.vlan.s.config.enable=1;
	    rrcp_config_commit_vlan_to_switch();
	}
	if ((strcasecmp(command,"vlan dot1q")==0)||(strcasecmp(command,"vlan dot1q force")==0)){
	    if (switchtypes[switchtype].chip_id==rtl8326){
		if (strcasecmp(command,"vlan dot1q force")==0){
		    cli_print(cli, "%% WARNING: Enabled IEEE 802.1Q VLANs on hardware, that do not supported them properly.");
		}else{
		    cli_print(cli, "%% IEEE 802.1Q VLANs not supported properly on this hardware. Use 'force' to persevere.");
		    return CLI_ERROR;
		}
	    }else{
	        if (!swconfig.vlan.s.config.dot1q || !swconfig.vlan.s.config.enable){
		    int i,port,port_phys;
		    swconfig.vlan_vid[0]=1;
		    swconfig.vlan_entry.bitmap[0]=0xffffffff;
		    for(i=1;i<32;i++){
			swconfig.vlan_vid[i]=0;
			swconfig.vlan_entry.bitmap[i]=0;
		    }
		    for(port=1;port<=switchtypes[switchtype].num_ports;port++){
			port_phys=map_port_number_from_logical_to_physical(port);
			swconfig.vlan.s.port_vlan_index[port_phys]=0;
		    }
		    swconfig.vlan_port_insert_vid.bitmap=0;
		    swconfig.vlan.s.config.dot1q=1;
		    swconfig.vlan.s.config.enable=1;
		    rrcp_config_commit_vlan_to_switch();
		}
	    }
	}
    }
    return CLI_OK;
}

int cmd_config_vlan_leaky(struct cli_def *cli, char *command, char *argv[], int argc)
{
    if (argc>0){
	if (strcmp(argv[0],"?")==0){
	    cli_print(cli, "  <cr>");
	}else{
	    cli_print(cli, "%% Invalid input detected.");
	}
    }else{
	if (strcasecmp(command,"no vlan leaky arp")==0) swconfig.vlan.s.config.arp_leaky=0;
	if (strcasecmp(command,"vlan leaky multicast")==0) swconfig.vlan.s.config.multicast_leaky=1;
	if (strcasecmp(command,"no vlan leaky multicast")==0) swconfig.vlan.s.config.multicast_leaky=0;
	if (strcasecmp(command,"vlan leaky unicast")==0) swconfig.vlan.s.config.unicast_leaky=1;
	if (strcasecmp(command,"no vlan leaky unicast")==0) swconfig.vlan.s.config.unicast_leaky=0;
	rtl83xx_setreg16(0x030b,swconfig.vlan.raw[0]);
    }
    return CLI_OK;
}

int cmd_config_vlan_drop(struct cli_def *cli, char *command, char *argv[], int argc)
{
    if (argc>0){
	if (strcmp(argv[0],"?")==0){
	    cli_print(cli, "  <cr>");
	}else{
	    cli_print(cli, "%% Invalid input detected.");
	}
    }else{
	if (strcasecmp(command,"vlan drop untagged_frames")==0) swconfig.vlan.s.config.drop_untagged_frames=1;
	if (strcasecmp(command,"no vlan drop untagged_frames")==0) swconfig.vlan.s.config.drop_untagged_frames=0;
	if (strcasecmp(command,"vlan drop invalid_vid")==0) swconfig.vlan.s.config.ingress_filtering=1;
	if (strcasecmp(command,"no vlan drop invalid_vid")==0) swconfig.vlan.s.config.ingress_filtering=0;
	rtl83xx_setreg16(0x030b,swconfig.vlan.raw[0]);
    }
    return CLI_OK;
}

int cmd_config_qos(struct cli_def *cli, char *command, char *argv[], int argc)
{
    if (argc>0){
	if (strcmp(argv[0],"?")==0){
	    cli_print(cli, "  <cr>");
	}else{
	    cli_print(cli, "%% Invalid input detected.");
	}
    }else{
	if (strcasecmp(command,"mls qos trust dscp")==0) swconfig.qos_config.config.dscp_enable=1;
	if (strcasecmp(command,"no mls qos trust dscp")==0) swconfig.qos_config.config.dscp_enable=0;
	if (strcasecmp(command,"mls qos trust cos")==0) swconfig.qos_config.config.cos_enable=1;
	if (strcasecmp(command,"no mls qos trust cos")==0) swconfig.qos_config.config.cos_enable=0;
	if (strcasecmp(command,"no wrr-queue ratio")==0) swconfig.qos_config.config.wrr_ratio=3;
	rtl83xx_setreg16(0x0400,swconfig.qos_config.raw);
    }
    return CLI_OK;
}

int cmd_config_qos_wrr_queue_ratio(struct cli_def *cli, char *command, char *argv[], int argc)
{
    if (argc>0){
	if (strcmp(argv[0],"?")==0){
	    int i;
	    for (i=0;i<4;i++){
		cli_print(cli, "%s",wrr_ratio_text[i]);
	    }
	}else{
	    int i,hit;
	    hit=0;
	    for (i=0;i<4;i++){
		if (strcmp(wrr_ratio_text[i],argv[0])==0){
		    swconfig.qos_config.config.wrr_ratio=i;
		    hit=1;
		}
	    }
	    if (hit){
		rtl83xx_setreg16(0x0400,swconfig.qos_config.raw);
	    }else{
		cli_print(cli, "%% Invalid input detected.");
	    }
	}
    }else{
	cli_print(cli, "%% Please specify ratio");
    }
    return CLI_OK;
}

int cmd_config_flowcontrol(struct cli_def *cli, char *command, char *argv[], int argc)
{
    if (argc>0){
	if (strcmp(argv[0],"?")==0){
	    cli_print(cli, "  <cr>");
	}else{
	    cli_print(cli, "%% Invalid input detected.");
	}
    }else{
	if (strcasecmp(command,"flowcontrol dot3x")==0) swconfig.port_config_global.config.flow_dot3x_disable=0;
	if (strcasecmp(command,"no flowcontrol dot3x")==0) swconfig.port_config_global.config.flow_dot3x_disable=1;
	if (strcasecmp(command,"flowcontrol backpressure")==0) swconfig.port_config_global.config.flow_backpressure_disable=0;
	if (strcasecmp(command,"no flowcontrol backpressure")==0) swconfig.port_config_global.config.flow_backpressure_disable=1;
	if (strcasecmp(command,"flowcontrol ondemand-disable")==0) swconfig.qos_config.config.flow_ondemand_disable=1;
	if (strcasecmp(command,"no flowcontrol ondemand-disable")==0) swconfig.qos_config.config.flow_ondemand_disable=0;
	rtl83xx_setreg16(0x0607,swconfig.port_config_global.raw);
	rtl83xx_setreg16(0x0400,swconfig.qos_config.raw);
    }
    return CLI_OK;
}

int cmd_config_stormcontrol(struct cli_def *cli, char *command, char *argv[], int argc)
{
    if (argc>0){
	if (strcmp(argv[0],"?")==0){
	    cli_print(cli, "  <cr>");
	}else{
	    cli_print(cli, "%% Invalid input detected.");
	}
    }else{
	if (strcasecmp(command,"no storm-control broadcast")==0) {
	    swconfig.port_config_global.config.storm_control_broadcast_disable=1;
	    swconfig.port_config_global.config.storm_control_broadcast_strict=0;
	}
	if (strcasecmp(command,"storm-control broadcast relaxed")==0) {
	    swconfig.port_config_global.config.storm_control_broadcast_disable=0;
	    swconfig.port_config_global.config.storm_control_broadcast_strict=0;
	}
	if (strcasecmp(command,"storm-control broadcast strict")==0) {
	    swconfig.port_config_global.config.storm_control_broadcast_disable=0;
	    swconfig.port_config_global.config.storm_control_broadcast_strict=1;
	}
	if (strcasecmp(command,"no storm-control multicast")==0) {
	    swconfig.port_config_global.config.storm_control_multicast_strict=0;
	}
	if (strcasecmp(command,"storm-control multicast")==0) {
	    swconfig.port_config_global.config.storm_control_multicast_strict=1;
	}
	rtl83xx_setreg16(0x0607,swconfig.port_config_global.raw);
    }
    return CLI_OK;
}

int cmd_config_spanning_tree(struct cli_def *cli, char *command, char *argv[], int argc)
{
    if (argc>0){
	if (strcmp(argv[0],"?")==0){
	    cli_print(cli, "  <cr>");
	}else{
	    cli_print(cli, "%% Invalid input detected.");
	}
    }else{
	if (strcasecmp(command,"spanning-tree bpdufilter enable")==0) swconfig.alt_config.s.config.stp_filter=1;
	if ((strcasecmp(command,"no spanning-tree bpdufilter enable")==0)||
	    (strcasecmp(command,"spanning-tree bpdufilter disable")==0)) swconfig.alt_config.s.config.stp_filter=0;
	rtl83xx_setreg16(0x0300,swconfig.alt_config.raw);
    }
    return CLI_OK;
}

int cmd_config_end(struct cli_def *cli, char *command, char *argv[], int argc)
{
    if (argc>0){
	if (strcmp(argv[0],"?")==0){
	    cli_print(cli, "  <cr>");
	}else{
	    cli_print(cli, "%% Invalid input detected.");
	}
    }else{
	cli_set_configmode(cli, MODE_EXEC, NULL);
    }
    return CLI_OK;
}

void cmd_config_register_commands(struct cli_def *cli)
{
    struct cli_command *c,*c2,*c3,*no;

    no=cli_register_command(cli, NULL, "no", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Negate a command or set its defaults");

    cli_register_command(cli, NULL, "version", cmd_config_version, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "");

    cli_register_command(cli, NULL, "hostname", cmd_config_hostname, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Set system's network host name");

    c=cli_register_command(cli, NULL, "mac-address-table", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Configure the MAC address table");
    cli_register_command(cli, c, "aging-time", cmd_config_mac_aging, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Set MAC address table entry maximum age");

    { // ip config
	struct cli_command *ip,*ip_igmp,*no_ip,*no_ip_igmp;
	ip=cli_register_command(cli, NULL, "ip", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Global IP configuration subcommands");
	ip_igmp=cli_register_command(cli, ip, "igmp", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "IGMP global configuration");
	cli_register_command(cli, ip_igmp, "snooping", cmd_config_ip_igmp_snooping, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Global IGMP Snooping enable");

	no_ip=cli_register_command(cli, no, "ip", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Global IP configuration subcommands");
	no_ip_igmp=cli_register_command(cli, no_ip, "igmp", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "IGMP global configuration");
	cli_register_command(cli, no_ip_igmp, "snooping", cmd_config_ip_igmp_snooping, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Global IGMP Snooping disable");
    }

    { // rrcp config
	c=cli_register_command(cli, NULL, "rrcp", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Global RRCP configuration subcommand");
	cli_register_command(cli, c, "enable", cmd_config_rrcp, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Enable RRCP");
	c2=cli_register_command(cli, no, "rrcp", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Global RRCP configuration subcommand");
	cli_register_command(cli, c2, "enable", cmd_config_rrcp, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Enable RRCP");

	c3=cli_register_command(cli, c, "echo", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Global RRCP Echo Protocol configuration subcommand");
	cli_register_command(cli, c3, "enable", cmd_config_rrcp, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Enable RRCP Echo Protocol");
	c3=cli_register_command(cli, c2, "echo", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Global RRCP Echo Protocol configuration subcommand");
	cli_register_command(cli, c3, "enable", cmd_config_rrcp, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Enable RRCP Echo Protocol");

	c3=cli_register_command(cli, c, "loop-detect", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Global RRCP-based loop detection configuration subcommand");
	cli_register_command(cli, c3, "enable", cmd_config_rrcp, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Enable RRCP-based loop detection");
	c3=cli_register_command(cli, c2, "loop-detect", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Global RRCP-based loop detection configuration subcommand");
	cli_register_command(cli, c3, "enable", cmd_config_rrcp, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Enable RRCP-based loop detection");

	cli_register_command(cli, c, "authkey", cmd_config_rrcp_authkey, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Set new RRCP Authentication key");
    }

    { // vlan config
	struct cli_command *vlan,*no_vlan,*vlan_leaky,*no_vlan_leaky,*vlan_drop,*no_vlan_drop;

	vlan=cli_register_command(cli, NULL, "vlan", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Global VLAN mode");
	no_vlan=cli_register_command(cli, no, "vlan", cmd_config_vlan, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Vlan commands");
	cli_register_command(cli, vlan, "portbased", cmd_config_vlan, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Enable port-based VLANs only");
	c2=cli_register_command(cli, vlan, "dot1q", cmd_config_vlan, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Enable full IEEE 802.1Q tagged VLANs");
	cli_register_command(cli, c2, "force", cmd_config_vlan, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Enable IEEE 802.1Q VLANs even on buggy hardware");

	vlan_leaky=cli_register_command(cli, vlan, "leaky", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Allow certain type of packets to be switched beetween VLANs");
	no_vlan_leaky=cli_register_command(cli, no_vlan, "leaky", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Disallow certain type of packets to be switched beetween VLANs");
	cli_register_command(cli, vlan_leaky, "arp", cmd_config_vlan_leaky, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Allow ARP packets to be switched beetween VLANs");
	cli_register_command(cli, no_vlan_leaky, "arp", cmd_config_vlan_leaky, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Disallow ARP packets to be switched beetween VLANs");
	cli_register_command(cli, vlan_leaky, "multicast", cmd_config_vlan_leaky, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Allow multicast packets to be switched beetween VLANs");
	cli_register_command(cli, no_vlan_leaky, "multicast", cmd_config_vlan_leaky, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Disallow multicast packets to be switched beetween VLANs");
	cli_register_command(cli, vlan_leaky, "unicast", cmd_config_vlan_leaky, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Allow unicast packets to be switched beetween VLANs");
	cli_register_command(cli, no_vlan_leaky, "unicast", cmd_config_vlan_leaky, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Disallow unicast packets to be switched beetween VLANs");

	vlan_drop=cli_register_command(cli, vlan, "drop", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Allow dropping certain types of non-conforming packets");
	no_vlan_drop=cli_register_command(cli, no_vlan, "drop", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Disallow dropping certain types of non-conforming packets");
	cli_register_command(cli, vlan_drop, "untagged_frames", cmd_config_vlan_drop, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Drop ALL untagged packets, on all ports");
	cli_register_command(cli, no_vlan_drop, "untagged_frames", cmd_config_vlan_drop, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Do not drop untagged packets");
	cli_register_command(cli, vlan_drop, "invalid_vid", cmd_config_vlan_drop, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Drop tagged frames with VID, that is invalid on this port");
	cli_register_command(cli, no_vlan_drop, "invalid_vid", cmd_config_vlan_drop, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Do not drop tagged frames with VID, that is invalid on this port");
    }
    
    { // qos config
	struct cli_command *mls,*no_mls,*qos,*no_qos,*trust,*no_trust;

	mls=cli_register_command(cli, NULL, "mls", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Multi-Layer Switch configuration");
	no_mls=cli_register_command(cli, no, "mls", cmd_config_qos, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Multi-Layer Switch configuration");
	qos=cli_register_command(cli, mls, "qos", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Global Quality-of-Service configuration");
	no_qos=cli_register_command(cli, no_mls, "qos", cmd_config_qos, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Global Quality-of-Service configuration");
	trust=cli_register_command(cli, qos, "trust", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Quality-of-Service trust configuration");
	no_trust=cli_register_command(cli, no_qos, "trust", cmd_config_qos, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Quality-of-Service trust configuration");
	cli_register_command(cli, trust, "dscp", cmd_config_qos, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Trust DSCP(TOS) value in IP header of incoming packets");
	cli_register_command(cli, no_trust, "dscp", cmd_config_qos, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Do not trust DSCP(TOS) value in IP header of incoming packets");
	cli_register_command(cli, trust, "cos", cmd_config_qos, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Trust CoS (802.1p) tag value of incoming packets");
	cli_register_command(cli, no_trust, "cos", cmd_config_qos, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Do not trust CoS (802.1p) tag value of incoming packets");

	c=cli_register_command(cli, NULL, "wrr-queue", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Configure Weighed Round-Robin queue");
	cli_register_command(cli, c, "ratio", cmd_config_qos_wrr_queue_ratio, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Configure ratio of high-priority vs. low-priority traffic to pass");
	c=cli_register_command(cli, no, "wrr-queue", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Configure Weighed Round-Robin queue");
	cli_register_command(cli, c, "ratio", cmd_config_qos, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Configure default high-priority vs. low-priority traffic ratio");
    }

    { // flowcontrol config
	struct cli_command *flowcontrol,*no_flowcontrol;

	flowcontrol=cli_register_command(cli, NULL, "flowcontrol", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Flow control configuration");
	no_flowcontrol=cli_register_command(cli, no, "flowcontrol", cmd_config_flowcontrol, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Flow control configuration");
	cli_register_command(cli, flowcontrol, "dot3x", cmd_config_flowcontrol, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Enable IEEE 802.3x flow control");
	cli_register_command(cli, no_flowcontrol, "dot3x", cmd_config_flowcontrol, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Disable IEEE 802.3x flow control");
	cli_register_command(cli, flowcontrol, "backpressure", cmd_config_flowcontrol, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Enable half-duplex back pressure flow control");
	cli_register_command(cli, no_flowcontrol, "backpressure", cmd_config_flowcontrol, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Disable half-duplex back pressure flow control");
	cli_register_command(cli, flowcontrol, "ondemand-disable", cmd_config_flowcontrol, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Allow on demand temporary disabling all flow control on port, where high-priority frame has arrived - use with care!");
	cli_register_command(cli, no_flowcontrol, "ondemand-disable", cmd_config_flowcontrol, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Do not allow on demand disabling of flow control");
    }

    { // storm-control config
	struct cli_command *stormcontrol,*no_stormcontrol,*stormcontrol_broadcast;

	stormcontrol=cli_register_command(cli, NULL, "storm-control", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Storm-contol (preventing certain excessive traffic) configuration");
	no_stormcontrol=cli_register_command(cli, no, "storm-control", cmd_config_flowcontrol, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Storm-contol (preventing certain excessive traffic) configuration");
	stormcontrol_broadcast=cli_register_command(cli, stormcontrol, "broadcast", cmd_config_stormcontrol, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Storm-control on broadcast traffic");
	cli_register_command(cli, stormcontrol_broadcast, "relaxed", cmd_config_stormcontrol, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Enable relaxed storm-control on broadcast traffic");
	cli_register_command(cli, stormcontrol_broadcast, "strict", cmd_config_stormcontrol, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Enable strict storm-control on broadcast traffic");
	cli_register_command(cli, no_stormcontrol, "broadcast", cmd_config_stormcontrol, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Disable storm-control on broadcast traffic");
	cli_register_command(cli, stormcontrol, "multicast", cmd_config_stormcontrol, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Enable strict storm-control on multicast traffic");
	cli_register_command(cli, no_stormcontrol, "multicast", cmd_config_stormcontrol, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Disable storm-control on multicast traffic");
    }

    { // spanning-tree config
	struct cli_command *spanning_tree,*no_spanning_tree;

	spanning_tree=cli_register_command(cli, NULL, "spanning-tree", NULL, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Spanning Tree Subsystem");
	no_spanning_tree=cli_register_command(cli, no, "spanning-tree", cmd_config_spanning_tree, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Spanning Tree Subsystem");
	c=cli_register_command(cli, spanning_tree, "bpdufilter", cmd_config_spanning_tree, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Don't send or receive BPDUs on this interface");
	cli_register_command(cli, c, "enable", cmd_config_spanning_tree, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Enable BPDU filtering for this interface");
	cli_register_command(cli, c, "disable", cmd_config_spanning_tree, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Disable BPDU filtering for this interface");
	c=cli_register_command(cli, no_spanning_tree, "bpdufilter", cmd_config_spanning_tree, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Don't send or receive BPDUs on this interface");
	cli_register_command(cli, c, "enable", cmd_config_spanning_tree, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Disable BPDU filtering for this interface");
    }
    
    cli_register_command(cli, NULL, "end", cmd_config_end, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "Exit from configure mode");
}
