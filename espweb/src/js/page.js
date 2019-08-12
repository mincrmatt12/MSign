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
			}
		}
	}

	componentDidMount() {
		this.beginLoad();
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
						let idx = key[key.length - 1];
						let prop = key[0] == 's' ? (key[1] == 't' ? 'stopids' : 'names') : 'dirtags';
						this.setState((s, _) => {
							s.ttc[prop].splice(idx, 0, value);
							return s;
						});
						break;
				}
			}
		}
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
						<Nav.Link disabled={!this.state.dirty}>save</Nav.Link>
						<Nav.Link >reboot</Nav.Link>
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
