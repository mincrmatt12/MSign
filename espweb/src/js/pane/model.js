import React from 'react'
import FormControl from 'react-bootstrap/FormControl'
import InputGroup from 'react-bootstrap/InputGroup'
import Form from 'react-bootstrap/Form'
import Row from 'react-bootstrap/Row'
import Col from 'react-bootstrap/Col'
import Card from 'react-bootstrap/Card'
import Container from 'react-bootstrap/Container'
import Button from 'react-bootstrap/Button'
import ListGroup from 'react-bootstrap/ListGroup'
import Alert from 'react-bootstrap/Alert'
import Modal from 'react-bootstrap/Modal'
import * as THREE from "three";
import { OrbitControls } from "three/examples/jsm/controls/OrbitControls";
import ResizeObserver from 'resize-observer-polyfill';

function ModelPaneListEntry(props) {
	return (<ListGroup.Item active={props.modelPresent && props.selected ? true : null} onClick={props.onClick} action disabled={!props.modelPresent}>
		<Form inline className="mb-0">
			<Form.Check className={props.builtin ? "" : "border-right pr-2 hr-gray"}>
				<Form.Check.Input checked={props.enabled} onChange={(e) => {props.onChangeState(e.target.checked);}}/>
				<Form.Check.Label>{props.name ? props.name : "<none>"}</Form.Check.Label>
				</Form.Check>
				{props.builtin ? null : (<Button className="force-enabled ml-2" variant="dark" onClick={props.onChangeModel}>add model</Button>)}
			</Form>
		</ListGroup.Item>);
}

ModelPaneListEntry.defaultProps = {
	onClick: () => {},
	enabled: false,
	selected: false,
	onChangeState: (v) => {},
	modelPresent: false,
	builtin: false,
	onChangeModel: () => {}
};

class ModelRenderer extends React.Component {
	componentDidMount() {
		this.generateScene(this.props.modelBuffer);
		this.startAnimationLoop();

		this.obsv = new ResizeObserver(e => {this.handleWindowResize()});
		this.obsv.observe(this.el);
	}

	componentWillUnmount() {
		window.cancelAnimationFrame(this.requestID);
		this.controls.dispose();
		this.renderer.dispose();
	}

	generateScene(ab) {
		const width = this.el.clientWidth;
		const height = width / 2;

		this.scene = new THREE.Scene();
		this.camera = new THREE.PerspectiveCamera(
			90,
			width / height,
			0.01, // near plane
			1000 // far plane
		);
		this.camera.position.z = 1;
		this.controls = new OrbitControls(this.camera, this.el);
		this.renderer = new THREE.WebGLRenderer();
		this.renderer.setSize(width, height);
		this.el.appendChild(this.renderer.domElement); // mount using React ref

		let dv = new DataView(ab, 0, 2);
		this.triangleCount = dv.getUint16(0, true);

		let fv = new Float32Array(ab.slice(2));
		let geometry = new THREE.Geometry();
		for (let i = 0; i < this.triangleCount; ++i) {
			geometry.vertices.push(
				new THREE.Vector3(-fv[i*12 + 0], fv[i*12 + 1], fv[i*12 + 2]),
				new THREE.Vector3(-fv[i*12 + 3], fv[i*12 + 4], fv[i*12 + 5]),
				new THREE.Vector3(-fv[i*12 + 6], fv[i*12 + 7], fv[i*12 + 8])
			);

			let face = new THREE.Face3(i*3, i*3 + 1, i*3 + 2);
			face.color.setRGB(
				fv[i*12 + 9] / 256.0, fv[i*12 + 10] / 256.0, fv[i*12 + 11] / 256.0
			);
			geometry.faces.push(
				face
			);
		}

		geometry.computeFaceNormals();
		geometry.computeVertexNormals();

		const material = new THREE.MeshBasicMaterial({
			wireframe: true,
			vertexColors: THREE.FaceColors,
			wireframeLinewidth: 3.5,
		});

		this.mesh = new THREE.Mesh(geometry, material);
		this.scene.add(this.mesh);
	}

	startAnimationLoop = () => {
		this.renderer.render(this.scene, this.camera);
		this.requestID = window.requestAnimationFrame(this.startAnimationLoop);
	}

	handleWindowResize = () => {
		const width = this.el.clientWidth;
		const height = width / 2;

		this.renderer.setSize(width, height);
		this.camera.aspect = width / height;

		// Note that after making changes to most of camera properties you have to call
		// .updateProjectionMatrix for the changes to take effect.
		this.camera.updateProjectionMatrix();
	};
	
	render() {
		return <div ref={ref => (this.el = ref)} />;
	}
}

class ModelPropertiesRenderer extends ModelRenderer {
	generateScene(ab) {
		super.generateScene(ab);

		const cambox_geom = new THREE.BoxGeometry(1, 1, 1);
		const cambox_mat = new THREE.MeshBasicMaterial({
			color: new THREE.Color(1, 0.2, 0.2),
			transparent: true,
			opacity: 0.5,
			wireframe: true
		});
		this.cambox = new THREE.Mesh(cambox_geom, cambox_mat);

		const focpoint_geom = new THREE.SphereGeometry(0.025);
		const focpoint_mat = new THREE.MeshBasicMaterial({
			color: new THREE.Color(1, 1, 1),
		});

		this.focpoints = [new THREE.Mesh(focpoint_geom, focpoint_mat),
			new THREE.Mesh(focpoint_geom, focpoint_mat),
			new THREE.Mesh(focpoint_geom, focpoint_mat)
		];

		this.mesh.add(this.cambox);
		for (let i of this.focpoints) {
			this.mesh.add(i);
		}

		this.computeTransforms();
	}

	computeTransforms() {
		// setup positions of focpoints
		
		let i = 0;
		for (;i < this.props.focuses.length; ++i) {
			if (this.props.focuses[i].some(Number.isNaN)) break;
			this.focpoints[i].position.set(...this.props.focuses[i]);
			this.focpoints[i].position.setX(-this.focpoints[i].position.x);
			this.focpoints[i].visible = true;
		}
		for (; i < 3; ++i) {
			this.focpoints[i].visible = false;
		}

		if (this.props.minpos.some(Number.isNaN) || this.props.maxpos.some(Number.isNaN)) {
			this.cambox.visible = false;
			return;
		}
		this.cambox.visible = true;
		
		// compute size of box
		const minpos = new THREE.Vector3(...this.props.minpos);
		const maxpos = new THREE.Vector3(...this.props.maxpos);
		let mx = minpos.x
		minpos.setX(-maxpos.x);
		maxpos.setX(-mx);

		const size = maxpos.clone().sub(minpos);
		const pos  = minpos.add(size.clone().divideScalar(2));

		this.cambox.position.copy(pos);
		this.cambox.scale.copy(size);
	}

	startAnimationLoop = () => {
		this.computeTransforms();
		this.renderer.render(this.scene, this.camera);
		this.requestID = window.requestAnimationFrame(this.startAnimationLoop);
	}
};

function ThreeComponentEdit(props) {
	return (
		<Form>
			{props.label}
			<Form.Row>
				<Col>
					<Form.Control type="number" step={props.step} defaultValue={props.x} onChange={(e) => props.onChange([Number.parseFloat(e.target.value), props.y, props.z])}/>
				</Col>
				<Col>
					<Form.Control type="number" step={props.step} defaultValue={props.y} onChange={(e) => props.onChange([props.x, Number.parseFloat(e.target.value), props.z])}/>
				</Col>
				<Col>
					<Form.Control type="number" step={props.step} defaultValue={props.z} onChange={(e) => props.onChange([props.x, props.y, Number.parseFloat(e.target.value)])}/>
				</Col>
			</Form.Row>
		</Form>
	);
}

ThreeComponentEdit.defaultProps = {
	step: 0.1,
	label: ""
}

class ModelPane extends React.Component {
	constructor(props) {
		super(props);

		this.state = {
			model: null,
			activeEntry: 0,
			loading: true,
			presence: [false, false],
			dwModel: -1,

			uploadModal: false,
			uploadModel: null,
			uploadSlot: 0
		};
	}

	componentDidMount() {
		this.refreshPresence();
	}

	refreshPresence() {
		fetch("/a/mpres.json")
			.then((x) => {
				if (!x.ok) throw new Error();
				return x;
			})
			.then((x) => x.json())
			.then((x) => {this.setState({presence: [x.m0, x.m1], loading: false});})
			.catch(() => {this.setState({loading: false});})
		;
	}

	upload(p, val) {
		let v_s = this.props['configState'];
		v_s[p] = val;
		this.props['updateState'](v_s);
	}

	uploadInner(p, i, val) {
		let v_s = this.props['configState'];
		v_s[p][i] = val;
		this.props['updateState'](v_s);
	}

	removeFocus(i0, i1) {
		let v_s = this.props['configState'];
		v_s.focuses[i0].splice(i1, 1);
		this.props['updateState'](v_s);
	}

	addFocus(i0) {
		let v_s = this.props['configState'];
		v_s.focuses[i0].push([NaN, NaN, NaN]);
		this.props['updateState'](v_s);
	}

	uploadFocus(i0, i1, v) {
		let v_s = this.props['configState'];
		v_s.focuses[i0][i1] = v;
		this.props['updateState'](v_s);
	}

	finishUpload(i, ab) {
		// Load the model from i

		this.setState({uploadModal: true, uploadSlot: i, uploadModel: ab});
	}

	finishUploadClose(doIt) {
		this.setState({uploadModal: false, uploadModel: null});

		if (!doIt) {
			return;
		}

		let fd = new FormData();
		fd.append(this.state.uploadSlot == 0 ? "model" : "model1", new Blob([this.state.uploadModel]));

		fetch('/a/newmodel', {
			method: 'POST',
			body: fd
		}).then((resp) => {
			if (resp.ok) {
				alert("sent model, reboot to see it onscreen");
				this.refreshPresence();
			}
			else {
				alert("failed to upload");
			}
		});


	}

	uploadNewModel(i) {
		let input = document.createElement('input');
		input.type = 'file';
		input.accept = "bin";

		input.onchange = (e => { 
			var file = e.target.files[0]; 

			var reader = new FileReader();
			reader.readAsArrayBuffer(file);

			reader.onload = readerEvent => {
				this.finishUpload(i, readerEvent.target.result);
			}

		});

		input.click();
	}

	componentDidUpdate(prevProps, prevState) {
		console.log(prevState, this.state);
		if (prevState.activeEntry != this.state.activeEntry || prevState.presence[this.state.activeEntry] != this.state.presence[this.state.activeEntry]) {
			this.setState({dwModel: -1});

			fetch(this.state.activeEntry == 0 ? "/a/model.bin" : "/a/model1.bin")
				.then((x) => {
					if (!x.ok) throw new Error();
					return x;
				})
				.then((x) => x.arrayBuffer())
				.then((ab) => {
					this.setState({dwModel: this.state.activeEntry, model: ab});
				});
		}
	}

	render() {
		const ae = this.state.activeEntry;

		return <div>
			<hr className="hr-gray" />
			{this.state.loading ? <Alert variant='info'>loading...</Alert> : null}
			<ListGroup>
				<ModelPaneListEntry name={this.props.configState.names[0]} selected={ae == 0} modelPresent={this.state.presence[0]} enabled={this.props.configState.enabled[0]} 
					onClick={() => this.setState({activeEntry: 0})}
					onChangeState={(v) => this.uploadInner('enabled', 0, v)} 
					onChangeModel={() => this.uploadNewModel(0)}
				/>
				<ModelPaneListEntry name={this.props.configState.names[1]} selected={ae == 1} modelPresent={this.state.presence[1]} enabled={this.props.configState.enabled[1]} 
					onClick={() => this.setState({activeEntry: 1})} 
					onChangeState={(v) => this.uploadInner('enabled', 1, v)}
					onChangeModel={() => this.uploadNewModel(0)}
				/>
				<ModelPaneListEntry name="builtin" modelPresent={true} builtin={true} enabled={this.props.configState.enabled[2]} 
					onChangeState={(v) => this.uploadInner('enabled', 2, v)} />
			</ListGroup>
			{this.state.presence[ae] && (<div>
				<hr className="hr-gray" />
				<Form>
					<Form.Group controlId="model-name">
						<Form.Label>name</Form.Label>
						<Form.Control type="text" value={this.props.configState.names[ae]} placeholder="<none>" onChange={(e) => this.uploadInner("names", ae, e.target.value)} />
					</Form.Group>
				</Form>
				<ThreeComponentEdit key={10 + ae} label="camera minpos" onChange={(v) => this.uploadInner("minposes", ae, v)}
					x={this.props.configState.minposes[ae][0]} y={this.props.configState.minposes[ae][1]} z={this.props.configState.minposes[ae][2]}/>
				<ThreeComponentEdit key={20 + ae} label="camera maxpos" onChange={(v) => this.uploadInner("maxposes", ae, v)} 
					x={this.props.configState.maxposes[ae][0]} y={this.props.configState.maxposes[ae][1]} z={this.props.configState.maxposes[ae][2]}/>
				<hr className="hr-gray" />
				<p>focus points</p>
				{[...this.props.configState.focuses[ae].keys()].map((x) => (
					<Row key={x}>
						<Col sm="11" xs="10">
							<ThreeComponentEdit key={x*10 + 30 + ae} onChange={(v) => {this.uploadFocus(ae, x, v);}}
								x={this.props.configState.focuses[ae][x][0]} y={this.props.configState.focuses[ae][x][1]} z={this.props.configState.focuses[ae][x][2]}/>
						</Col>
						<Col sm="1" xs="2" className="pl-0">
							<Button variant="danger" disabled={this.props.configState.focuses[ae].length <= 1} onClick={() => this.removeFocus(ae, x)} className="w-100">X</Button>
						</Col>
					</Row>
				))}

				<Button variant="success" disabled={this.props.configState.focuses[ae].length == 3} onClick={() => this.addFocus(ae)}>add focus</Button>
				<hr className="hr-gray" />
				{this.state.dwModel != ae ? null : 
						(<ModelPropertiesRenderer focuses={this.props.configState.focuses[ae]} maxpos={this.props.configState.maxposes[ae]} minpos={this.props.configState.minposes[ae]} modelBuffer={this.state.model} />)
				}</div>)}

			<hr className="hr-gray" />

			<Modal size="lg" centered show={this.state.uploadModal} onHide={() => this.finishUploadClose(false)}>
				<Modal.Header>
					<Modal.Title>confirm model</Modal.Title>
				</Modal.Header>
				<Modal.Body>
					<p>does this look good?</p>
					{this.state.uploadModal ? (<ModelRenderer modelBuffer={this.state.uploadModel} />) : null}
				</Modal.Body>
				<Modal.Footer>
					<Button variant="secondary" onClick={() => this.finishUploadClose(false)}>nah</Button>
					<Button variant="primary" onClick={() => this.finishUploadClose(true)}>yes, upload it</Button>
				</Modal.Footer>
			</Modal>
		</div>
	}
}

export default ModelPane;
