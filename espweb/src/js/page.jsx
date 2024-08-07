import ReactDOM from 'react-dom'
import React from 'react'
import Navbar from 'react-bootstrap/Navbar'
import Nav from 'react-bootstrap/Nav'
import Container from 'react-bootstrap/Container'
import Row from 'react-bootstrap/Row'
import Col from 'react-bootstrap/Col'
import Card from 'react-bootstrap/Card'
import Alert from 'react-bootstrap/Alert'
import {BrowserRouter as Router, Route} from 'react-router-dom'
import {LinkContainer} from 'react-router-bootstrap'

import GlobalPane from "./pane/global"
import TransitPane from "./pane/transit"
import WeatherPane from "./pane/weather"
import ApiPane from "./pane/apikeys"
import ScCfgPane from "./pane/sc"
import UpdatePane from "./pane/upd"
import ModelPane from "./pane/model"
import CfgPullPane from "./pane/cfgpull"
import RawPane from "./pane/raw"
import ParcelsPane from "./pane/parcels"
import OctoprintPane from "./pane/octoprint"
import WifiPane from "./pane/wifiadv"

import ConfigContext from "./ctx"
import BackendVersionContext from "./ver"

import _ from 'lodash';

function App() {
	const [loading, setLoading] = React.useState(true);
	const [dirty, setDirty] = React.useState(false);
	const [error, setError] = React.useState(false);
	const [cfg, setCfg] = React.useState({});
	const [fetchedVer, setFetchedVer] = React.useState("cfgserver");
	const [sleep, setSleep] = React.useState(false);

	React.useEffect(() => {
		fetch("/a/conf.json") 
			.then((resp) => {
				if (resp.ok) {
					return resp.json();
				}
				throw new Error("invalid response");
			})
			.then((dat) => {
				setCfg(dat);
				setLoading(false);
			})
			.catch(() => {
				setError(true);
				setLoading(false);
			});
	}, []);

	React.useEffect(() => {
		fetch("/a/sleep")
			.then((resp) => resp.text())
			.then((dat) => setSleep(dat == "true"))
			.catch(() => {});
	}, []);

	React.useEffect(() => {
		fetch("/a/version")
			.then((resp) => {
				if (resp.headers.get("Content-Type").startsWith("text/html")) return "cfgserver";
				if (resp.ok) {
					return resp.text();
				}
			})
			.then((text) => {setFetchedVer(text);})
			.catch(() => {});
	}, []);

	const saveConfig = () => {
		fetch("/a/conf.json", {
			body: JSON.stringify(cfg),
			method: "POST"
		}).then((resp) => {
			if (!resp.ok) alert("couldn't save...");
			else {
				alert("saved config");
				setDirty(false);
			}
		});
	};

	const nextSleepState = !sleep;

	const toggleSleep = () => {
		const url = nextSleepState ? "/a/sleepOn" : "/a/sleepOff";
		fetch(url, {method: "POST"})
			.then((resp) => {
				if (!resp.ok) alert("failed to sleep");
				else setSleep(nextSleepState);
			});
	};

	return (
		<Router basename="/">
			<Navbar variant="dark" bg="dark" expand="sm">
				<Container fluid>
					<Navbar.Brand>msign control panel</Navbar.Brand>
					<Nav variant="pills">
						<Nav.Item>
							<Nav.Link eventKey={1} href="#" disabled={!dirty} active={false} onClick={() => {saveConfig();}}>save</Nav.Link>
						</Nav.Item>
						<Nav.Item>
							<Nav.Link eventKey={2} href="#" disabled={loading} active={false} onClick={toggleSleep}>{nextSleepState ? "sleep" : "unsleep"}</Nav.Link>
						</Nav.Item>
						<Nav.Item>
							<Nav.Link eventKey={3} href="#" active={false} onClick={() => {fetch("/a/reboot");}}>reboot</Nav.Link>
						</Nav.Item>
					</Nav>
				</Container>
			</Navbar>

			<Container className="contbrd">
				<Row>
					<Col sm="4" md="3" lg="2" className="mb-2">
						<Card bg="dark">
							<Nav variant="pills" className="flex-column msign-left-nav">
								<LinkContainer to="/" exact>
									<Nav.Link>global</Nav.Link>
								</LinkContainer>
								<LinkContainer to="/wifi">
									<Nav.Link>wifi</Nav.Link>
								</LinkContainer>
								<LinkContainer to="/ttc">
									<Nav.Link>transit</Nav.Link>
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
								<LinkContainer to="/model">
									<Nav.Link>model</Nav.Link>
								</LinkContainer>
								<LinkContainer to="/parcels">
									<Nav.Link>parcels</Nav.Link>
								</LinkContainer>
								<LinkContainer to="/octoprint">
									<Nav.Link>octoprint</Nav.Link>
								</LinkContainer>
								<LinkContainer to="/cfgpull">
									<Nav.Link>cfgpull</Nav.Link>
								</LinkContainer>
								<LinkContainer to="/upd">
									<Nav.Link>sysupdate</Nav.Link>
								</LinkContainer>
								<hr className="hr-darkgray my-1" />
								<LinkContainer to="/raw">
									<Nav.Link>raw</Nav.Link>
								</LinkContainer>
							</Nav>
						</Card>
					</Col>
					<BackendVersionContext.Provider value={fetchedVer}>
						<Col sm="8" md="9" lg="10" className="mb-2">
							{error ? <Alert variant='warning'>failed to load config; running in test mode -- no changes will be saved.</Alert> : ""}
							{loading ? <Alert variant='info'>loading...</Alert> : <ConfigContext.Provider value={[cfg, (path, value) => {
								if (path === "*") {
									setCfg(value);
								}
								else {
									if (value instanceof Function) {
										value = value(_.get(cfg, path));
									}
									setCfg(_.setWith(_.clone(cfg), path, value, _.clone));
								}
								setDirty(true);
							}]}>
								<Route path="/" exact>
									<GlobalPane  />
								</Route>
								<Route path="/ttc">
									<TransitPane />
								</Route>
								<Route path="/weather">
									<WeatherPane />
								</Route>
								<Route path="/apikey">
									<ApiPane     />
								</Route>
								<Route path="/sccfg">
									<ScCfgPane   />
								</Route>
								<Route path="/model">
									<ModelPane   />
								</Route>
								<Route path="/parcels">
									<ParcelsPane />
								</Route>
								<Route path="/wifi">
									<WifiPane />
								</Route>
								<Route path="/cfgpull">
									<CfgPullPane />
								</Route>
								<Route path="/octoprint">
									<OctoprintPane />
								</Route>
								<Route path="/upd" component={UpdatePane} />
								<Route path="/raw" component={RawPane} />
							</ConfigContext.Provider>}
						</Col>
					</BackendVersionContext.Provider>
				</Row>
			</Container>
		</Router>
	);
}

const mount = document.getElementById("mount");
ReactDOM.render(<App />, mount);
