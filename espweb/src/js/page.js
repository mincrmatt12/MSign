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

import "regenerator-runtime/runtime";

import GlobalPane from "./pane/global"
import TTCPane from "./pane/ttc"
import WeatherPane from "./pane/weather"
import ApiPane from "./pane/apikeys"
import ScCfgPane from "./pane/sc"
import UpdatePane from "./pane/upd"
import ModelPane from "./pane/model"

import ConfigContext from "./ctx"

import _ from 'lodash';

function App() {
	const [loading, setLoading] = React.useState(true);
	const [dirty, setDirty] = React.useState(false);
	const [error, setError] = React.useState(false);
	const [cfg, setCfg] = React.useState({});

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

	return (
		<Router basename="/">
			<Navbar variant="dark" bg="dark">
				<Container fluid>
					<Navbar.Brand>msign control panel</Navbar.Brand>
					<Nav variant="pills">
						<Nav.Link eventKey={1} href="#" disabled={!dirty} active={false} onSelect={() => {saveConfig();}}>save</Nav.Link>
						<Nav.Link eventKey={2} href="#" active={false} onSelect={() => {fetch("/a/reboot");}}>reboot</Nav.Link>
					</Nav>
				</Container>
			</Navbar>

			<Container className="contbrd">
				<Row>
					<Col xs="5" sm="4" md="3" lg="2">
						<Card bg="dark">
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
								<LinkContainer to="/model">
									<Nav.Link>model</Nav.Link>
								</LinkContainer>
								<LinkContainer to="/upd">
									<Nav.Link>sysupdate</Nav.Link>
								</LinkContainer>
							</Nav>
						</Card>
					</Col>
					<Col xs="7" sm="8" md="9" lg="10">
						{error ? <Alert variant='warning'>failed to load config; running in test mode -- no changes will be saved.</Alert> : ""}
						{loading ? <Alert variant='info'>loading...</Alert> : <ConfigContext.Provider value={[cfg, (path, value) => {
							setCfg(_.setWith(_.clone(cfg), path, value, _.clone));
							setDirty(true);
						}]}>
							<Route path="/" exact>
								<GlobalPane  />
							</Route>
							<Route path="/ttc">
								<TTCPane     />
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
							<Route path="/upd" component={UpdatePane} />
						</ConfigContext.Provider>}
					</Col>
				</Row>
			</Container>
		</Router>
	);
}

const mount = document.getElementById("mount");
ReactDOM.render(<App />, mount);
