import "../css/boot.scss"
import ReactDOM from 'react-dom'
import React from 'react'
import Navbar from 'react-bootstrap/Navbar'
import Nav from 'react-bootstrap/Nav'
import Form from 'react-bootstrap/Form'
import Button from 'react-bootstrap/Form'
import Container from 'react-bootstrap/Container'
import Row from 'react-bootstrap/Row'
import Col from 'react-bootstrap/Col'
import Card from 'react-bootstrap/Card'
import Alert from 'react-bootstrap/Alert'
import {BrowserRouter as Router, Route} from 'react-router-dom'
import {LinkContainer} from 'react-router-bootstrap'
import "core-js/stable";
import "regenerator-runtime/runtime";

import GlobalPane from "./pane/global"
import TTCPane from "./pane/ttc"
import WeatherPane from "./pane/weather"
import ApiPane from "./pane/apikeys"
import ScCfgPane from "./pane/sc"
import UpdatePane from "./pane/upd"

class App extends React.Component {
	constructor(props) {
		super(props);

		/*
		 * the state of the App object contains the entire config file in a js readable format.
		 * this is then passed to each "config pane"
		 * these all update the state, which is serialized when you press save
		 * that's then uploaded to the server, and unlike the upd interface it simply writes it to the sd card.
		 *
		 * a bunch of things support live updating though, so those both update the global state _and_ use their own interface
		 */

		this.state = {
			// overall info
			loaded: false,
			dirty: false,
			error: false,

			ttc: {
				stopids: [],
				names: [],
				dirtags: []
			},
			
			global: {
				ssid: "",
				psk: "",
				ntpserver: "",
				configuser: "",
				configpass: ""
			},

			apikeys: {
				darksky: ""
			},

			weather: {
				latitude: 0,
				longitude: 0
			},

			sccfg: {
				ttc: {mode: 0, startTime: NaN, endTime: NaN, duration: 12000},
				weather: {mode: 0, startTime: NaN, endTime: NaN, duration: 12000},
				model: {mode: 0, startTime: NaN, endTime: NaN, duration: 12000},
			}
		}
	}

	componentDidMount() {
		this.beginLoad();
	}

	serialize() {
		let result = "";
		// global params
		result += "ssid=" + this.state.global.ssid + "\n";
		result += "psk=" + this.state.global.psk + "\n";

		// optional global params
		if (this.state.global.ntpserver !== "")
			result += "ntpserver=" + this.state.global.ntpserver + "\n";
		if (this.state.global.configuser !== "")
			result += "configuser=" + this.state.global.configuser + "\n";
		if (this.state.global.configpass !== "")
			result += "configpass=" + this.state.global.configpass + "\n";

		// required weather api parameters
		result += "wapilat=" + this.state.weather.latitude.toString() + "\n";
		result += "wapilon=" + this.state.weather.longitude.toString() + "\n";

		// api keys
		result += "wapikey=" + this.state.apikeys.darksky + "\n";

		// ttc
		for (let i = 0; i < this.state.ttc.dirtags.length; ++i) {
			result += "stopid" + (i + 1).toString() + "=" + this.state.ttc.stopids[i] + "\n";
			result += "dirtag" + (i + 1).toString() + "=" + this.state.ttc.dirtags[i] + "\n";
			result += "shortname" + (i + 1).toString() + "=" + this.state.ttc.names[i] + "\n";
		}

		// sccfg
		let needsNewSc = false;
		for (let prop in this.state.sccfg) {
			const name = {'ttc': 'ttc', 'weather': 'weather', 'model': '3d'}[prop];
			const p = this.state.sccfg[prop];

			if (p.duration != 12000) needsNewSc = true;

			if (p.mode == 0) continue;
			else if (p.mode == 1) {
				result += "sc" + name + "=off\n";
			}
			else {
				result += "sc" + name + "=" + p.startTime.toString() + "," + p.endTime.toString() + "\n";
			}
		}
		if (needsNewSc) {
			result += "sctimes=" + this.state.sccfg.ttc.duration.toString() + "," +
								   this.state.sccfg.weather.duration.toString() + "," +
								   this.state.sccfg.model.duration.toString() + "\n";
		}

		return result;
	}

	updateFromConfigTxt(txt) {
		for (const line of txt.split('\n')) {
			if (line === "") continue;
			else {
				let datum = line.split('=');
				let key = datum[0];
				let value = datum[1];

				switch (key) {
					case 'ssid':
					case 'psk':
					case 'ntpserver':
					case 'configuser':
					case 'configpass':
						this.setState((s, _) => {s.global[key] = value; return s});
						break;
					case 'wapikey':
						this.setState((s, _) => {s.apikeys.darksky = value; return s});
						break;
					case 'wapilat':
					case 'wapilon':
						this.setState((s, _) => {
							let d = Number.parseFloat(value);
							s.weather[key == 'wapilat' ? 'latitude' : 'longitude'] = d;
							return s;
						});
						break;
					case 'stopid1':
					case 'stopid2':
					case 'stopid3':
					case 'shortname1':
					case 'shortname2':
					case 'shortname3':
					case 'dirtag1':
					case 'dirtag2':
					case 'dirtag3':
						{
							let idx = key[key.length - 1];
							let prop = key[0] == 's' ? (key[1] == 't' ? 'stopids' : 'names') : 'dirtags';
							this.setState((s, _) => {
								s.ttc[prop].splice(idx, 0, value);
								return s;
							});
						}
						break;
					case 'scttc':
					case 'scweather':
					case 'sc3d':
						{
							let prop = key.substr(2);
							if (prop == '3d') prop = 'model';
							if (value == "on") break;
							if (value == "off") this.setState((s, _) => {s.sccfg[prop].mode = 1; return s;});
							else {
								let startstop = value.split(",");
								this.setState((s, _) => {
									s.sccfg[prop].mode = 2;
									s.sccfg[prop].startTime = Number.parseInt(startstop[0]);
									s.sccfg[prop].endTime   = Number.parseInt(startstop[1]);
									return s;
								});
							}
						}
						break;
					case 'sctimes':
						this.setState((s, _) => {
							let values = value.split(",").map((x) => Number.parseInt(x));

							s.sccfg.ttc.duration = values[0];
							s.sccfg.weather.duration = values[1];
							s.sccfg.model.duration = values[2];
						});
						break;
				}
			}
		}
	}

	saveConfig() {
		fetch("/a/conf.txt", {
			body: this.serialize(),
			method: "POST"
		}).then((resp) => {
			if (!resp.ok) alert("couldn't save...");
			else {
				alert("saved config");
				this.setState({dirty: false});
			}
		});
	}

	reboot() {
		fetch("/a/reboot");
	}

	beginLoad() {
		fetch("/a/conf.txt") 
			.then((resp) => {
				if (resp.ok) {
					return resp.text();
				}
				throw new Error("invalid response");
			})
			.then((txt) => {
				this.updateFromConfigTxt(txt);
				this.setState({loaded: true});
			})
			.catch(() => {
				this.setState({
					error: true,
					loaded: true
				});
			});
	}

	createUpdateFunc(piece) {
		return (y) => {this.setState((s, _) => {
			s[piece] = y; 
			s.dirty = true;
			return s;
		})};
	}

	render() {
		return (
			<Router basename="/">
				<Navbar bg="light">
					<Navbar.Brand>msign control panel</Navbar.Brand>
					<Nav variant="pills">
						<Nav.Link eventKey={1} href="#" disabled={!this.state.dirty} active={false} onSelect={() => {this.saveConfig();}}>save</Nav.Link>
						<Nav.Link eventKey={2} href="#" active={false} onSelect={() => {this.reboot();}}>reboot</Nav.Link>
					</Nav>
				</Navbar>

				<Container className="contbrd">
					<Row>
						<Col xs="5" sm="4" md="3" lg="2">
							<Card bg="light">
								<Nav variant="pills" className="flex-column">
									<LinkContainer to="/" exact>
										<Nav.Link>global</Nav.Link>
									</LinkContainer>
									<LinkContainer to="/ttc">
										<Nav.Link>ttc</Nav.Link>
									</LinkContainer>
									<LinkContainer to="/weather">
										<Nav.Link>weather</Nav.Link>
									</LinkContainer>
									<LinkContainer to="/apikey">
										<Nav.Link>apikeys</Nav.Link>
									</LinkContainer>
									<LinkContainer to="/sccfg">
										<Nav.Link>sccfg</Nav.Link>
									</LinkContainer>
									<LinkContainer to="/upd">
										<Nav.Link>sysupdate</Nav.Link>
									</LinkContainer>
								</Nav>
							</Card>
						</Col>
						<Col xs="7" sm="8" md="9" lg="10">
							{this.state.error ? <Alert variant='warning'>failed to load config; running in test mode -- no changes will be saved.</Alert> : ""}
							{!this.state.loaded ? <Alert variant='info'>loading...</Alert> : <div>
								<Route path="/" exact     render={(props) => {
									return <GlobalPane  configState={this.state.global } updateState={this.createUpdateFunc('global')} />
								}} />
								<Route path="/ttc"        render={(props) => {
									return <TTCPane     configState={this.state.ttc    } updateState={this.createUpdateFunc('ttc')} />
								}} />
								<Route path="/weather"    render={(props) => {
									return <WeatherPane configState={this.state.weather} updateState={this.createUpdateFunc('weather')} />
								}} />
								<Route path="/apikey"     render={(props) => {
									return <ApiPane     configState={this.state.apikeys} updateState={this.createUpdateFunc('apikeys')} />
								}} />
								<Route path="/sccfg"      render={(props) => {
									return <ScCfgPane   configState={this.state.sccfg  } updateState={this.createUpdateFunc('sccfg')} />
								}} />


								<Route path="/upd" component={UpdatePane} />
							</div>}
						</Col>
					</Row>
				</Container>
			</Router>
		);
	}
}

const mount = document.getElementById("mount");
ReactDOM.render(<App />, mount);
