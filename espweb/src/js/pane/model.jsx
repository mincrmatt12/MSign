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
import {
	Scene,
	PerspectiveCamera,
	WebGLRenderer,
	BufferGeometry,
	BufferAttribute,
	MeshBasicMaterial,
	DoubleSide,
	Mesh,
	BoxGeometry,
	Color,
	SphereGeometry,
	Vector3,
} from "three";
import { OrbitControls } from "three/examples/jsm/controls/OrbitControls";
import ConfigContext from "../ctx";
import * as _ from "lodash-es";

function ModelPaneListEntry(props) {
	return (<ListGroup.Item active={props.modelPresent && props.selected ? true : null} className="d-flex align-items-center" onClick={props.onClick} action disabled={!props.modelPresent}>
			<Form.Check className={props.builtin ? "" : "border-end pe-2 hr-gray"} label={props.name ? props.name : "<none>"} checked={props.enabled} onChange={(e) => {props.onChangeState(e.target.checked);}}/>
			{props.builtin ? null : (<Button className="force-enabled ms-2" variant="light" onClick={props.onChangeModel}>add model</Button>)}
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

		this.scene = new Scene();
		this.camera = new PerspectiveCamera(
			90,
			width / height,
			0.01, // near plane
			1000 // far plane
		);
		this.camera.position.z = 1;
		this.controls = new OrbitControls(this.camera, this.el);
		this.renderer = new WebGLRenderer();
		this.renderer.setSize(width, height);
		this.el.appendChild(this.renderer.domElement); // mount using React ref

		let dv = new DataView(ab, 0, 2);
		this.triangleCount = dv.getUint16(0, true);

		let fv = new Float32Array(ab.slice(2));
		let pts = new Float32Array(this.triangleCount * 9);
		let cols = new Float32Array(this.triangleCount * 9);
		let geometry = new BufferGeometry();
		for (let i = 0; i < this.triangleCount; ++i) {
			for (let c = 0; c < 9; ++c) {
				pts[i*9 + c] = (c % 3 == 0 ? -1 : 1) * fv[i*12 + c];
				cols[i*9 + c] = fv[i * 12 + 9 + c % 3] / 255.0;
			}
		}

		geometry.setAttribute("position", new BufferAttribute(pts, 3));
		geometry.setAttribute("color", new BufferAttribute(cols, 3));

		const material = new MeshBasicMaterial({
			vertexColors: true,
			side: DoubleSide
		});

		this.mesh = new Mesh(geometry, material);
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

		const cambox_geom = new BoxGeometry(1, 1, 1);
		const cambox_mat = new MeshBasicMaterial({
			color: new Color(1, 0.2, 0.2),
			transparent: true,
			opacity: 0.5,
			wireframe: true
		});
		this.cambox = new Mesh(cambox_geom, cambox_mat);

		const focpoint_geom = new SphereGeometry(0.025);
		const focpoint_mat = new MeshBasicMaterial({
			color: new Color(1, 1, 1),
		});

		this.focpoints = [new Mesh(focpoint_geom, focpoint_mat),
			new Mesh(focpoint_geom, focpoint_mat),
			new Mesh(focpoint_geom, focpoint_mat)
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
		const minpos = new Vector3(...this.props.minpos);
		const maxpos = new Vector3(...this.props.maxpos);
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
	return (<>
		{props.label && <Form.Label>{props.label}</Form.Label>}
		<Row>
			<Col>
				<Form.Control type="number" step={props.step} defaultValue={props.x} onChange={(e) => props.onChange([Number.parseFloat(e.target.value), props.y, props.z])}/>
			</Col>
			<Col>
				<Form.Control type="number" step={props.step} defaultValue={props.y} onChange={(e) => props.onChange([props.x, Number.parseFloat(e.target.value), props.z])}/>
			</Col>
			<Col>
				<Form.Control type="number" step={props.step} defaultValue={props.z} onChange={(e) => props.onChange([props.x, props.y, Number.parseFloat(e.target.value)])}/>
			</Col>
		</Row>
	</>);
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

	toVecArray(v) {
		if (v === undefined) return [0, 0, 0];
		return [v.x, v.y, v.z];
	}

	render() {
		const [cfg, updateCfg] = this.context;
		const ae =  this.state.activeEntry;

		return <div>
			<hr className="hr-gray" />
			{this.state.loading ? <Alert variant='info'>loading...</Alert> : null}
			<ListGroup>
				<ModelPaneListEntry name={_.get(cfg, ["model", "sd", 0, "name"], "")} selected={ae == 0} modelPresent={this.state.presence[0]} enabled={_.get(cfg, "model.enabled[0]", false)} 
					onClick={() => this.setState({activeEntry: 0})}
					onChangeState={(v) => updateCfg("model.enabled[0]", v)} 
					onChangeModel={() => this.uploadNewModel(0)}
				/>
				<ModelPaneListEntry name={_.get(cfg, ["model", "sd", 1, "name"], "")} selected={ae == 1} modelPresent={this.state.presence[1]} enabled={_.get(cfg, "model.enabled[1]", false)} 
					onClick={() => this.setState({activeEntry: 1})} 
					onChangeState={(v) => updateCfg("model.enabled[1]", v)}
					onChangeModel={() => this.uploadNewModel(1)}
				/>
				<ModelPaneListEntry name="builtin" modelPresent={true} builtin={true} enabled={_.get(cfg, "model.enabled[2]", true)} 
					onChangeState={(v) => this.updateCfg("model.enabled[2]", v)} />
			</ListGroup>
			{this.state.presence[ae] && (<div>
				<hr className="hr-gray" />
				<Form.Group className="my-2" controlId="model-name">
					<Form.Label>name</Form.Label>
					<Form.Control type="text" value={_.get(cfg, ["model", "sd", ae, "name"], "")} placeholder="<none>" onChange={(e) => updateCfg(["model", "sd", ae, "name"], e.target.value)} />
				</Form.Group>
				<ThreeComponentEdit label="camera minpos" onChange={(v) => updateCfg(["model", "sd", ae, "cam", "min"], {
					x: v[0], y: v[1], z: v[2]
				})}
					x={_.get(cfg, ["model", "sd", ae, "cam", "min", "x"], 0.0)} y={_.get(cfg, ["model", "sd", ae, "cam", "min", "y"], 0.0)} z={_.get(cfg, ["model", "sd", ae, "cam", "min", "z"], 0.0)}/>
				<ThreeComponentEdit label="camera maxpos" onChange={(v) => updateCfg(["model", "sd", ae, "cam", "max"], {
					x: v[0], y: v[1], z: v[2]
				})}
					x={_.get(cfg, ["model", "sd", ae, "cam", "max", "x"], 0.0)} y={_.get(cfg, ["model", "sd", ae, "cam", "max", "y"], 0.0)} z={_.get(cfg, ["model", "sd", ae, "cam", "max", "z"], 0.0)}/>
				<hr className="hr-gray" />
				<Form.Label className="d-block">focus points</Form.Label>
				{_.get(cfg, ["model", "sd", ae, "cam", "focuses"], []).map((foc, idx) => (
					<Row key={idx} className="my-2">
						<Col sm="11" xs="10">
							<ThreeComponentEdit onChange={(v) => {updateCfg(["model", "sd", ae, "cam", "focuses", idx], {
								x: v[0], y: v[1], z: v[2]
							})}}
								x={foc.x} y={foc.y} z={foc.z}/>
						</Col>
						<Col sm="1" xs="2" className="ps-0">
							<Button variant="danger" disabled={_.get(cfg, ["model", "sd", ae, "cam", "focuses"], []).length <= 1} onClick={() => updateCfg(["model", "sd", ae, "cam", "focuses"], 
								_.filter(_.get(cfg, ["model", "sd", ae, "cam", "focuses"]), (_, idx2) => idx2 != idx))} className="w-100">X</Button>
						</Col>
					</Row>
				))}

				<Button variant="success" disabled={_.get(cfg, ["model", "sd", ae, "cam", "focuses"], []).length >= 3} onClick={() => {
					updateCfg(["model", "sd", ae, "cam", "focuses"], _.concat(_.get(cfg, ["model", "sd", ae, "cam", "focuses"], []), {"x": 0, "y": 0, "z": 0}));
				}}>add focus</Button>
				<hr className="hr-gray" />
				{this.state.dwModel != ae ? null : 
						(<ModelPropertiesRenderer focuses={_.get(cfg, ["model", "sd", ae, "cam", "focuses"], []).map(this.toVecArray)} maxpos={
							this.toVecArray(_.get(cfg, ["model", "sd", ae, "cam", "max"]))
						} minpos={this.toVecArray(_.get(cfg, ["model", "sd", ae, "cam", "min"]))} modelBuffer={this.state.model} />)
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

ModelPane.contextType = ConfigContext;

export default ModelPane;
