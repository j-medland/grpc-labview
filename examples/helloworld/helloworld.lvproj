<?xml version='1.0' encoding='UTF-8'?>
<Project Type="Project" LVVersion="19008000">
	<Property Name="CCSymbols" Type="Str">NI_GRPC_LV_SUPPORT_DEBUG,True;</Property>
	<Property Name="NI.LV.All.SaveVersion" Type="Str">19.0</Property>
	<Property Name="NI.LV.All.SourceOnly" Type="Bool">true</Property>
	<Property Name="NI.Project.Description" Type="Str"></Property>
	<Item Name="My Computer" Type="My Computer">
		<Property Name="server.app.propertiesEnabled" Type="Bool">true</Property>
		<Property Name="server.control.propertiesEnabled" Type="Bool">true</Property>
		<Property Name="server.tcp.enabled" Type="Bool">false</Property>
		<Property Name="server.tcp.port" Type="Int">0</Property>
		<Property Name="server.tcp.serviceName" Type="Str">My Computer/VI Server</Property>
		<Property Name="server.tcp.serviceName.default" Type="Str">My Computer/VI Server</Property>
		<Property Name="server.vi.callsEnabled" Type="Bool">true</Property>
		<Property Name="server.vi.propertiesEnabled" Type="Bool">true</Property>
		<Property Name="specify.custom.address" Type="Bool">false</Property>
		<Item Name="helloworld_client" Type="Folder">
			<Item Name="helloworld_client.lvlib" Type="Library" URL="../helloworld_client/helloworld_client.lvlib"/>
		</Item>
		<Item Name="helloworld_server" Type="Folder">
			<Item Name="helloworld_server.lvlib" Type="Library" URL="../helloworld_server/helloworld_server.lvlib"/>
		</Item>
		<Item Name="Dependencies" Type="Dependencies">
			<Item Name="vi.lib" Type="Folder">
				<Item Name="1D String Array to Delimited String.vi" Type="VI" URL="/&lt;vilib&gt;/AdvancedString/1D String Array to Delimited String.vi"/>
				<Item Name="Application Directory.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/file.llb/Application Directory.vi"/>
				<Item Name="Complete Call.vi" Type="VI" URL="/&lt;vilib&gt;/gRPC/LabVIEW gRPC Library/Server API/Message Requests/Complete Call.vi"/>
				<Item Name="Error Cluster From Error Code.vi" Type="VI" URL="/&lt;vilib&gt;/Utility/error.llb/Error Cluster From Error Code.vi"/>
				<Item Name="Get Server DLL Path.vi" Type="VI" URL="/&lt;vilib&gt;/gRPC/LabVIEW gRPC Library/Server API/Server/Get Server DLL Path.vi"/>
				<Item Name="grpcId.ctl" Type="VI" URL="/&lt;vilib&gt;/gRPC/LabVIEW gRPC Library/Shared/grpcId.ctl"/>
				<Item Name="LVNumericRepresentation.ctl" Type="VI" URL="/&lt;vilib&gt;/numeric/LVNumericRepresentation.ctl"/>
				<Item Name="NI_Data Type.lvlib" Type="Library" URL="/&lt;vilib&gt;/Utility/Data Type/NI_Data Type.lvlib"/>
				<Item Name="NI_FileType.lvlib" Type="Library" URL="/&lt;vilib&gt;/Utility/lvfile.llb/NI_FileType.lvlib"/>
				<Item Name="Read Call Request.vim" Type="VI" URL="/&lt;vilib&gt;/gRPC/LabVIEW gRPC Library/Server API/Message Requests/Read Call Request.vim"/>
				<Item Name="ServiceBase.lvclass" Type="LVClass" URL="/&lt;vilib&gt;/gRPC/LabVIEW gRPC Servicer/ServiceBase/ServiceBase.lvclass"/>
				<Item Name="TranslateGrpcError.vi" Type="VI" URL="/&lt;vilib&gt;/gRPC/LabVIEW gRPC Library/Shared/TranslateGrpcError.vi"/>
				<Item Name="Write Call Response.vim" Type="VI" URL="/&lt;vilib&gt;/gRPC/LabVIEW gRPC Library/Server API/Message Requests/Write Call Response.vim"/>
			</Item>
			<Item Name="grpc-lvsupport.lvlib" Type="Library" URL="../../../labview source/gRPC lv Support/grpc-lvsupport/grpc-lvsupport.lvlib"/>
			<Item Name="gRPC-servicer.lvlib" Type="Library" URL="../../../labview source/gRPC lv Servicer/gRPC-servicer.lvlib"/>
			<Item Name="labview_grpc_server.dll" Type="Document" URL="/&lt;resource&gt;/labview_grpc_server.dll"/>
		</Item>
		<Item Name="Build Specifications" Type="Build"/>
	</Item>
</Project>
